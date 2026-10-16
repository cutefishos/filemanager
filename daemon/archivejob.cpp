#include "archivejob.h"

#include <archive.h>
#include <archive_entry.h>
#include <zip.h>

#include "base/strings.h"
#include "base/uuid.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <regex>

#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string errorString(int code)
{
    return std::strerror(code);
}

static std::string fileNameOf(const std::string &path)
{
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string absoluteParent(const std::string &path)
{
    std::error_code error;
    const fs::path absolute = fs::absolute(path, error);
    return absolute.parent_path().string();
}

static bool existsNoFollow(const std::string &path)
{
    struct stat st;
    return ::lstat(path.c_str(), &st) == 0;
}

static bool isDirectory(const std::string &path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool makePath(const std::string &path)
{
    std::error_code error;
    fs::create_directories(path, error);
    return isDirectory(path);
}

static void removeFile(const std::string &path)
{
    ::unlink(path.c_str());
}

// A rename that never replaces an existing file.
static bool renameNoReplace(const std::string &from, const std::string &to)
{
    return ::renameat2(AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) == 0;
}

static std::string zipError(zip_t *archive)
{
    if (!archive)
        return "Unknown ZIP error";

    return zip_strerror(archive);
}

static std::string zipOpenError(int errorCode)
{
    zip_error_t error;
    zip_error_init_with_code(&error, errorCode);
    const std::string message = zip_error_strerror(&error);
    zip_error_fini(&error);
    return message;
}

using ArchiveReader = std::unique_ptr<struct archive, decltype(&archive_read_free)>;

static std::string archiveError(struct archive *reader)
{
    // An encrypted entry or header fails with a format-specific message.
    if (archive_read_has_encrypted_entries(reader) > 0)
        return "Password-protected archives are not supported";

    const char *message = archive_error_string(reader);
    return message ? std::string(message) : std::string("Unknown archive error");
}

static ArchiveReader openArchiveReader(const std::string &archivePath, std::string *error)
{
    ArchiveReader reader(archive_read_new(), &archive_read_free);
    if (!reader) {
        *error = "Could not allocate memory for the archive";
        return reader;
    }

    // Not format_all: formats such as mtree name files elsewhere on disk.
    archive_read_support_format_7zip(reader.get());
    archive_read_support_format_rar(reader.get());
    archive_read_support_format_rar5(reader.get());
    archive_read_support_format_tar(reader.get());
    archive_read_support_format_zip(reader.get());
    archive_read_support_format_empty(reader.get());
    // Raw bids lowest, so it only takes what no archive format claims.
    archive_read_support_format_raw(reader.get());

    archive_read_support_filter_bzip2(reader.get());
    archive_read_support_filter_compress(reader.get());
    archive_read_support_filter_gzip(reader.get());
    archive_read_support_filter_lzip(reader.get());
    archive_read_support_filter_lzma(reader.get());
    archive_read_support_filter_xz(reader.get());
    archive_read_support_filter_zstd(reader.get());

    if (archive_read_open_filename(reader.get(), archivePath.c_str(), 64 * 1024) != ARCHIVE_OK) {
        *error = archiveError(reader.get());
        reader.reset();
    }

    return reader;
}

static bool isArchiveVolume(const std::string &archivePath)
{
    static const std::regex volume(R"(\.(part\d+\.rar|7z\.\d+)$)", std::regex::icase);
    return std::regex_search(fileNameOf(archivePath), volume);
}

static std::string entryPathName(struct archive_entry *entry)
{
    if (const char *name = archive_entry_pathname_utf8(entry))
        return name;

    const char *name = archive_entry_pathname(entry);
    return name ? std::string(name) : std::string();
}

static std::string entryLinkTarget(struct archive_entry *entry)
{
    if (const char *target = archive_entry_symlink_utf8(entry))
        return target;

    const char *target = archive_entry_symlink(entry);
    return target ? std::string(target) : std::string();
}

static struct timespec entryModified(struct archive_entry *entry)
{
    struct timespec modified;
    modified.tv_sec = archive_entry_mtime(entry);
    modified.tv_nsec = archive_entry_mtime_nsec(entry);
    return modified;
}

static std::string temporaryPath(const std::string &parentPath, const std::string &prefix)
{
    return parentPath + "/." + prefix + '-' + cutefish::base::Uuid::generate().toString();
}

static bool isDirectoryEntry(const std::string &name)
{
    return name.ends_with('/');
}

static bool hasSymbolicLinkParent(const std::string &rootPath, const std::string &relativePath)
{
    const auto components = cutefish::base::strings::split(relativePath, '/',
                                                            cutefish::base::strings::SplitBehavior::SkipEmptyParts);
    std::string currentPath = rootPath;

    for (size_t index = 0; index + 1 < components.size(); ++index) {
        currentPath += '/' + components[index];
        struct stat st;
        if (::lstat(currentPath.c_str(), &st) != 0)
            break;
        if (S_ISLNK(st.st_mode))
            return true;
    }

    return false;
}

static std::string topLevelArchiveName(const std::string &archivePath)
{
    const size_t separator = archivePath.find('/');
    return separator == std::string::npos ? archivePath : archivePath.substr(0, separator);
}

ArchiveJob::ArchiveJob(Operation operation, std::vector<std::string> inputs, std::string outputPath)
    : m_operation(operation)
    , m_inputs(std::move(inputs))
    , m_outputPath(std::move(outputPath))
    , m_cancelRequested(false)
{
}

ArchiveJob::~ArchiveJob()
{
    cancel();
    if (m_thread.joinable())
        m_thread.join();
}

void ArchiveJob::start()
{
    m_thread = std::thread([this] { run(); });
}

void ArchiveJob::cancel()
{
    m_cancelRequested.store(true);
}

bool ArchiveJob::isCanceled() const
{
    return m_cancelRequested.load();
}

void ArchiveJob::progressChanged(int progress)
{
    if (onProgress)
        onProgress(progress);
}

void ArchiveJob::currentFileChanged(const std::string &fileName)
{
    if (onCurrentFile)
        onCurrentFile(fileName);
}

void ArchiveJob::run()
{
    std::string error;
    const bool success = m_operation == Compress ? compress(&error) : extract(&error);
    // Once the final rename has succeeded, the operation is committed. A
    // cancellation arriving in the tiny window before this function reads
    // the flag must not turn a completed operation into a false cancellation.
    const bool canceled = !success && isCanceled();

    if (onCompleted)
        onCompleted(success && !canceled, canceled, error, success && !canceled ? m_outputPath : std::string());
}

void ArchiveJob::emitProgress(uint64_t processed, uint64_t total)
{
    if (total == 0) {
        progressChanged(processed == 0 ? 0 : 100);
        return;
    }

    const double ratio = static_cast<double>(processed) / static_cast<double>(total);
    const int percent = std::clamp(static_cast<int>(ratio * 100.0), 0, 100);
    // Extraction reports every 64 KiB, and each report becomes a D-Bus
    // broadcast in the daemon; only a changed percentage is worth one.
    if (percent == m_lastPercent)
        return;
    m_lastPercent = percent;
    progressChanged(percent);
}

bool ArchiveJob::collectCompressionEntries(const std::string &sourcePath, const std::string &archivePath,
                                           std::vector<CompressionEntry> *entries, std::string *error) const
{
    if (isCanceled())
        return false;

    struct stat linkStat;
    if (::lstat(sourcePath.c_str(), &linkStat) != 0) {
        *error = "The source no longer exists: " + sourcePath;
        return false;
    }

    if (S_ISLNK(linkStat.st_mode)) {
        char target[16 * 1024];
        const ssize_t length = ::readlink(sourcePath.c_str(), target, sizeof(target));
        if (length <= 0) {
            *error = "Could not read symbolic link: " + sourcePath;
            return false;
        }

        entries->push_back({sourcePath, archivePath, false, 0, true, std::string(target, size_t(length))});
        return true;
    }

    if (S_ISDIR(linkStat.st_mode)) {
        const std::string directoryArchivePath = archivePath.ends_with('/') ? archivePath : archivePath + '/';
        entries->push_back({sourcePath, directoryArchivePath, true, 0, false, std::string()});

        std::vector<std::pair<bool, std::string>> children;
        if (DIR *directory = ::opendir(sourcePath.c_str())) {
            while (dirent *entry = ::readdir(directory)) {
                const std::string name = entry->d_name;
                if (name == "." || name == "..")
                    continue;
                children.push_back({!isDirectory(sourcePath + '/' + name), name});
            }
            ::closedir(directory);
        }
        // Folders first, then by name.
        std::sort(children.begin(), children.end());

        for (const auto &[file, name] : children) {
            if (!collectCompressionEntries(sourcePath + '/' + name, directoryArchivePath + name, entries, error))
                return false;
        }

        return true;
    }

    if (!S_ISREG(linkStat.st_mode)) {
        *error = "The source is not a regular file: " + sourcePath;
        return false;
    }

    entries->push_back({sourcePath, archivePath, false, uint64_t(linkStat.st_size), false, std::string()});
    return true;
}

bool ArchiveJob::compress(std::string *error)
{
    if (m_inputs.empty()) {
        *error = "No files were selected";
        return false;
    }

    std::vector<CompressionEntry> entries;
    for (const std::string &input : m_inputs) {
        if (!collectCompressionEntries(input, fileNameOf(input), &entries, error))
            return false;
    }

    if (isCanceled())
        return false;

    const std::string parentPath = absoluteParent(m_outputPath);
    if (!isDirectory(parentPath)) {
        *error = "The destination folder does not exist: " + parentPath;
        return false;
    }

    const std::string tempPath = temporaryPath(parentPath, "cutefish-archive");
    int openError = 0;
    zip_t *archive = zip_open(tempPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &openError);
    if (!archive) {
        removeFile(tempPath);
        *error = zipOpenError(openError);
        return false;
    }

    const auto fail = [&](std::string message) {
        *error = std::move(message);
        zip_discard(archive);
        removeFile(tempPath);
        return false;
    };

    if (zip_register_progress_callback_with_state(archive, 0.01, &ArchiveJob::zipProgressCallback, nullptr, this)
                != 0
        || zip_register_cancel_callback_with_state(archive, &ArchiveJob::zipCancelCallback, nullptr, this) != 0)
        return fail(zipError(archive));

    progressChanged(0);
    for (const CompressionEntry &entry : entries) {
        if (isCanceled())
            return fail({});

        // Keep the progress UI focused on the selected item. A folder may
        // contain many entries, but its internal archive paths are not useful
        // to show while the archive is being created.
        currentFileChanged(topLevelArchiveName(entry.archivePath));

        if (entry.directory) {
            if (zip_dir_add(archive, entry.archivePath.c_str(), ZIP_FL_ENC_UTF_8) < 0)
                return fail(zipError(archive));
            continue;
        }

        zip_source_t *source = nullptr;
        if (entry.symbolicLink) {
            const std::string &linkTarget = entry.linkTarget;
            const size_t bufferSize = std::max<size_t>(linkTarget.size(), 1);
            char *buffer = static_cast<char *>(std::malloc(bufferSize));
            if (!buffer)
                return fail("Could not allocate memory for symbolic link");

            std::memcpy(buffer, linkTarget.data(), linkTarget.size());
            source = zip_source_buffer(archive, buffer, linkTarget.size(), 1);
            if (!source)
                std::free(buffer);
        } else {
            source = zip_source_file(archive, entry.sourcePath.c_str(), 0, -1);
        }

        if (!source)
            return fail(zipError(archive));

        const zip_int64_t entryIndex = zip_file_add(archive, entry.archivePath.c_str(), source, ZIP_FL_ENC_UTF_8);
        if (entryIndex < 0) {
            zip_source_free(source);
            return fail(zipError(archive));
        }

        // A fast, low-level DEFLATE profile: quick archives without an
        // expensive maximum-compression pass.
        if (zip_set_file_compression(archive, static_cast<zip_uint64_t>(entryIndex), ZIP_CM_DEFLATE, 1) != 0)
            return fail(zipError(archive));

        if (entry.symbolicLink
            && zip_file_set_external_attributes(archive, static_cast<zip_uint64_t>(entryIndex), 0, ZIP_OPSYS_UNIX,
                                                static_cast<zip_uint32_t>(S_IFLNK | 0777) << 16)
                    != 0)
            return fail(zipError(archive));
    }

    if (isCanceled())
        return fail({});

    if (zip_close(archive) != 0)
        return fail(zipError(archive));

    if (isCanceled() || !renameNoReplace(tempPath, m_outputPath)) {
        removeFile(tempPath);
        if (!isCanceled())
            *error = "Could not create " + m_outputPath;
        return false;
    }

    progressChanged(100);
    return true;
}

bool ArchiveJob::archiveEntryPath(const std::string &archiveName, std::string *relativePath, bool *directory) const
{
    std::string name = archiveName;
    std::replace(name.begin(), name.end(), '\\', '/');

    *directory = isDirectoryEntry(name);
    while (name.ends_with('/'))
        name.pop_back();

    if (name.empty()) {
        relativePath->clear();
        return true;
    }

    if (name.starts_with('/') || (name.size() >= 2 && name[1] == ':') || name.find('\0') != std::string::npos)
        return false;

    std::vector<std::string> kept;
    for (const std::string &component :
         cutefish::base::strings::split(name, '/', cutefish::base::strings::SplitBehavior::SkipEmptyParts)) {
        if (component == "..")
            return false;
        if (component != ".")
            kept.push_back(component);
    }

    // Tar archives made from "." begin with a "./" entry for the root.
    *relativePath = cutefish::base::strings::join(kept, "/");
    return true;
}

bool ArchiveJob::extractEntryData(struct archive *reader, int fd, uint64_t total, std::string *error)
{
    char buffer[64 * 1024];
    while (true) {
        if (isCanceled())
            return false;

        const la_ssize_t bytesRead = archive_read_data(reader, buffer, sizeof(buffer));
        if (bytesRead < 0) {
            *error = archiveError(reader);
            return false;
        }

        if (bytesRead == 0)
            return true;

        la_ssize_t written = 0;
        while (written < bytesRead) {
            const ssize_t result = ::write(fd, buffer + written, static_cast<size_t>(bytesRead - written));
            if (result < 0) {
                if (errno == EINTR)
                    continue;

                *error = errorString(errno);
                return false;
            }
            written += result;
        }

        emitProgress(static_cast<uint64_t>(std::max<la_int64_t>(0, archive_filter_bytes(reader, -1))), total);
    }
}

bool ArchiveJob::extractSingleFile(struct archive *reader, bool empty, const std::string &parentPath,
                                   uint64_t total, std::string *error)
{
    const std::string tempPath = temporaryPath(parentPath, "cutefish-extract");
    const int fd = ::open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0) {
        *error = "Could not create " + fileNameOf(m_outputPath) + ": " + errorString(errno);
        return false;
    }

    currentFileChanged(fileNameOf(m_outputPath));

    const bool written = empty || extractEntryData(reader, fd, total, error);
    if (::close(fd) != 0 && written) {
        *error = errorString(errno);
        removeFile(tempPath);
        return false;
    }

    if (!written) {
        removeFile(tempPath);
        return false;
    }

    if (isCanceled() || !renameNoReplace(tempPath, m_outputPath)) {
        if (!isCanceled())
            *error = "Could not create " + m_outputPath;
        removeFile(tempPath);
        return false;
    }

    progressChanged(100);
    return true;
}

bool ArchiveJob::extract(std::string *error)
{
    if (m_inputs.size() != 1) {
        *error = "Only one archive can be extracted at a time";
        return false;
    }

    const std::string archivePath = m_inputs.front();
    if (isArchiveVolume(archivePath)) {
        *error = "Multi-part archives are not supported";
        return false;
    }

    const std::string parentPath = absoluteParent(m_outputPath);
    if (!isDirectory(parentPath)) {
        *error = "The destination folder does not exist: " + parentPath;
        return false;
    }

    ArchiveReader reader = openArchiveReader(archivePath, error);
    if (!reader)
        return false;

    // A compressed stream cannot be sized without decompressing it, so
    // progress follows how much of the archive file has been read.
    struct stat archiveStat;
    const uint64_t total = ::stat(archivePath.c_str(), &archiveStat) == 0 ? uint64_t(archiveStat.st_size) : 0;
    progressChanged(0);

    struct archive_entry *entry = nullptr;
    int status = archive_read_next_header(reader.get(), &entry);
    if (status < ARCHIVE_WARN) {
        *error = archiveError(reader.get());
        return false;
    }

    const int format = archive_format(reader.get()) & ARCHIVE_FORMAT_BASE_MASK;
    if (format == ARCHIVE_FORMAT_RAW || format == ARCHIVE_FORMAT_EMPTY) {
        // Without a compression filter the raw format would take any file.
        if (archive_filter_count(reader.get()) < 2) {
            *error = "Unrecognized archive format";
            return false;
        }

        return extractSingleFile(reader.get(), status == ARCHIVE_EOF, parentPath, total, error);
    }

    const std::string tempPath = temporaryPath(parentPath, "cutefish-extract");
    if (!makePath(tempPath)) {
        *error = "Could not create a temporary extraction folder";
        return false;
    }

    struct Cleanup {
        std::string path;
        bool active = true;
        ~Cleanup()
        {
            if (active) {
                std::error_code ignored;
                fs::remove_all(path, ignored);
            }
        }
    } cleanup {tempPath};

    struct PendingLink {
        std::string relativePath;
        std::string target;
    };
    struct PendingDirectory {
        std::string path;
        struct timespec modified;
    };
    std::vector<PendingLink> links;
    std::vector<PendingDirectory> directories;

    for (; status != ARCHIVE_EOF; status = archive_read_next_header(reader.get(), &entry)) {
        if (status < ARCHIVE_WARN) {
            *error = archiveError(reader.get());
            return false;
        }

        if (isCanceled())
            return false;

        const std::string name = entryPathName(entry);
        std::string relativePath;
        bool directory = false;
        if (!archiveEntryPath(name, &relativePath, &directory)) {
            *error = "Unsafe path in archive: " + name;
            return false;
        }

        if (archive_entry_is_encrypted(entry)) {
            *error = "Password-protected archives are not supported";
            return false;
        }

        if (archive_entry_hardlink(entry)) {
            *error = "Hard links in archives are not supported: " + name;
            return false;
        }

        const mode_t type = archive_entry_filetype(entry);
        if (type == AE_IFLNK) {
            const std::string target = entryLinkTarget(entry);
            if (directory || relativePath.empty()) {
                *error = "Invalid symbolic link path in archive: " + name;
                return false;
            }

            if (target.empty() || target.size() > 16 * 1024) {
                *error = "Invalid symbolic link target: " + relativePath;
                return false;
            }

            links.push_back({relativePath, target});
            continue;
        }

        if (type != AE_IFREG && type != AE_IFDIR) {
            *error = "Special files in archives are not supported: " + name;
            return false;
        }

        if (relativePath.empty())
            continue;

        const std::string destinationPath = tempPath + '/' + relativePath;
        // Set-user-ID and set-group-ID bits never survive extraction.
        const mode_t permissions = archive_entry_perm(entry) & 0777;
        currentFileChanged(relativePath);

        if (!makePath(absoluteParent(destinationPath))) {
            *error = "Could not create the parent folder for " + relativePath;
            return false;
        }

        if (type == AE_IFDIR || directory) {
            // A folder first met as a parent keeps the default mode.
            if ((::mkdir(destinationPath.c_str(), permissions | S_IRWXU) != 0 && errno != EEXIST)
                || !isDirectory(destinationPath)) {
                *error = "Could not create " + relativePath;
                return false;
            }

            if (archive_entry_mtime_is_set(entry))
                directories.push_back({destinationPath, entryModified(entry)});
            continue;
        }

        const int fd = ::open(destinationPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
                              permissions | S_IRUSR | S_IWUSR);
        if (fd < 0) {
            *error = "Could not create " + relativePath + ": " + errorString(errno);
            return false;
        }

        if (!extractEntryData(reader.get(), fd, total, error)) {
            ::close(fd);
            return false;
        }

        if (archive_entry_mtime_is_set(entry)) {
            const struct timespec times[2] = {{0, UTIME_OMIT}, entryModified(entry)};
            ::futimens(fd, times);
        }

        if (::close(fd) != 0) {
            *error = "Could not write " + relativePath + ": " + errorString(errno);
            return false;
        }
    }

    // Create symbolic links only after all regular files have been written.
    // A link inside an archive must never redirect extraction of a later file.
    for (const PendingLink &link : links) {
        if (isCanceled())
            return false;

        currentFileChanged(link.relativePath);

        const std::string destinationPath = tempPath + '/' + link.relativePath;
        if (hasSymbolicLinkParent(tempPath, link.relativePath)) {
            *error = "Symbolic link parent in archive: " + link.relativePath;
            return false;
        }

        if (!makePath(absoluteParent(destinationPath))) {
            *error = "Could not create the parent folder for " + link.relativePath;
            return false;
        }

        if (existsNoFollow(destinationPath)) {
            *error = "Duplicate path in archive: " + link.relativePath;
            return false;
        }

        if (::symlink(link.target.c_str(), destinationPath.c_str()) != 0) {
            *error = "Could not restore symbolic link: " + link.relativePath;
            return false;
        }
    }

    // Writing into a folder changes its time, so folders get theirs last.
    for (const PendingDirectory &directory : directories) {
        const struct timespec times[2] = {{0, UTIME_OMIT}, directory.modified};
        ::utimensat(AT_FDCWD, directory.path.c_str(), times, AT_SYMLINK_NOFOLLOW);
    }

    if (isCanceled() || !renameNoReplace(tempPath, m_outputPath)) {
        if (!isCanceled())
            *error = "Could not create " + m_outputPath;
        return false;
    }

    cleanup.active = false;
    progressChanged(100);
    return true;
}

void ArchiveJob::zipProgressCallback(zip_t *, double progress, void *state)
{
    auto *job = static_cast<ArchiveJob *>(state);
    if (!job)
        return;

    job->emitProgress(static_cast<uint64_t>(progress * 100.0), 100);
}

int ArchiveJob::zipCancelCallback(zip_t *, void *state)
{
    auto *job = static_cast<ArchiveJob *>(state);
    return job && job->isCanceled() ? 1 : 0;
}
