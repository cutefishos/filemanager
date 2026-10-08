#include "archivejob.h"

#include <archive.h>
#include <archive_entry.h>
#include <zip.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QUuid>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static QString zipError(zip_t *archive)
{
    if (!archive)
        return QStringLiteral("Unknown ZIP error");

    return QString::fromUtf8(zip_strerror(archive));
}

static QString zipOpenError(int errorCode)
{
    zip_error_t error;
    zip_error_init_with_code(&error, errorCode);
    const QString message = QString::fromUtf8(zip_error_strerror(&error));
    zip_error_fini(&error);
    return message;
}

using ArchiveReader = std::unique_ptr<struct archive, decltype(&archive_read_free)>;

static QString archiveError(struct archive *reader)
{
    // An encrypted entry or header fails with a format-specific message.
    if (archive_read_has_encrypted_entries(reader) > 0)
        return QStringLiteral("Password-protected archives are not supported");

    const char *message = archive_error_string(reader);
    return message ? QString::fromUtf8(message) : QStringLiteral("Unknown archive error");
}

static ArchiveReader openArchiveReader(const QString &archivePath, QString *error)
{
    ArchiveReader reader(archive_read_new(), &archive_read_free);
    if (!reader) {
        *error = QStringLiteral("Could not allocate memory for the archive");
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

    const QByteArray archiveName = QFile::encodeName(archivePath);
    if (archive_read_open_filename(reader.get(), archiveName.constData(), 64 * 1024) != ARCHIVE_OK) {
        *error = archiveError(reader.get());
        reader.reset();
    }

    return reader;
}

static bool isArchiveVolume(const QString &archivePath)
{
    static const QRegularExpression volume(QStringLiteral("\\.(part\\d+\\.rar|7z\\.\\d+)$"),
                                           QRegularExpression::CaseInsensitiveOption);
    return volume.match(QFileInfo(archivePath).fileName()).hasMatch();
}

static QString entryPathName(struct archive_entry *entry)
{
    if (const char *name = archive_entry_pathname_utf8(entry))
        return QString::fromUtf8(name);

    const char *name = archive_entry_pathname(entry);
    return name ? QFile::decodeName(name) : QString();
}

static QString entryLinkTarget(struct archive_entry *entry)
{
    if (const char *target = archive_entry_symlink_utf8(entry))
        return QString::fromUtf8(target);

    const char *target = archive_entry_symlink(entry);
    return target ? QFile::decodeName(target) : QString();
}

static struct timespec entryModified(struct archive_entry *entry)
{
    struct timespec modified;
    modified.tv_sec = archive_entry_mtime(entry);
    modified.tv_nsec = archive_entry_mtime_nsec(entry);
    return modified;
}

static QString temporaryPath(const QString &parentPath, const QString &prefix)
{
    return QDir(parentPath).filePath(QStringLiteral(".%1-%2")
                                         .arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

static bool isDirectoryEntry(const QString &name)
{
    return name.endsWith(QLatin1Char('/'));
}

static bool hasSymbolicLinkParent(const QString &rootPath, const QString &relativePath)
{
    const QStringList components = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString currentPath = rootPath;

    for (qsizetype index = 0; index + 1 < components.size(); ++index) {
        currentPath = QDir(currentPath).filePath(components.at(index));
        const QFileInfo info(currentPath);
        if (info.isSymLink())
            return true;

        if (!info.exists())
            break;
    }

    return false;
}

static QString topLevelArchiveName(const QString &archivePath)
{
    const qsizetype separator = archivePath.indexOf(QLatin1Char('/'));
    return separator < 0 ? archivePath : archivePath.left(separator);
}

ArchiveJob::ArchiveJob(Operation operation,
                       const QStringList &inputs,
                       const QString &outputPath,
                       QObject *parent)
    : QThread(parent)
    , m_operation(operation)
    , m_inputs(inputs)
    , m_outputPath(outputPath)
    , m_cancelRequested(false)
{
}

ArchiveJob::~ArchiveJob()
{
    cancel();
    wait();
}

void ArchiveJob::cancel()
{
    m_cancelRequested.store(true);
}

bool ArchiveJob::isCanceled() const
{
    return m_cancelRequested.load();
}

void ArchiveJob::run()
{
    QString error;
    const bool success = m_operation == Compress ? compress(&error) : extract(&error);
    // Once the final rename has succeeded, the operation is committed. A
    // cancellation arriving in the tiny window before this function reads
    // the flag must not turn a completed operation into a false cancellation.
    const bool canceled = !success && isCanceled();

    emit completed(success && !canceled, canceled, error,
                   success && !canceled ? m_outputPath : QString());
}

void ArchiveJob::emitProgress(quint64 processed, quint64 total)
{
    if (total == 0) {
        emit progressChanged(processed == 0 ? 0 : 100);
        return;
    }

    const double ratio = static_cast<double>(processed) / static_cast<double>(total);
    const int percent = qBound(0, static_cast<int>(ratio * 100.0), 100);
    // Extraction reports every 64 KiB, and each report becomes a D-Bus
    // broadcast in the daemon; only a changed percentage is worth one.
    if (percent == m_lastPercent)
        return;
    m_lastPercent = percent;
    emit progressChanged(percent);
}

bool ArchiveJob::collectCompressionEntries(const QString &sourcePath,
                                           const QString &archivePath,
                                           QList<CompressionEntry> *entries,
                                           QString *error) const
{
    if (isCanceled())
        return false;

    const QFileInfo info(sourcePath);
    if (!info.exists() && !info.isSymLink()) {
        *error = QStringLiteral("The source no longer exists: %1").arg(sourcePath);
        return false;
    }

    if (info.isSymLink()) {
#ifdef Q_OS_UNIX
        const QString linkTarget = info.symLinkTarget();
        if (linkTarget.isEmpty()) {
            *error = QStringLiteral("Could not read symbolic link: %1").arg(sourcePath);
            return false;
        }

        entries->append({sourcePath, archivePath, false, 0, true, linkTarget});
        return true;
#else
        *error = QStringLiteral("Symbolic links are not supported: %1").arg(sourcePath);
        return false;
#endif
    }

    if (info.isDir()) {
        const QString directoryArchivePath = archivePath.endsWith(QLatin1Char('/'))
                                                  ? archivePath
                                                  : archivePath + QLatin1Char('/');
        entries->append({sourcePath, directoryArchivePath, true, 0, false, QString()});

        QDir directory(sourcePath);
        const QFileInfoList children = directory.entryInfoList(
            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::Name);

        for (const QFileInfo &child : children) {
            const QString childArchivePath = directoryArchivePath + child.fileName();
            if (!collectCompressionEntries(child.absoluteFilePath(), childArchivePath, entries, error))
                return false;
        }

        return true;
    }

    if (!info.isFile()) {
        *error = QStringLiteral("The source is not a regular file: %1").arg(sourcePath);
        return false;
    }

    entries->append({sourcePath, archivePath, false, static_cast<quint64>(info.size()), false, QString()});
    return true;
}

bool ArchiveJob::compress(QString *error)
{
    if (m_inputs.isEmpty()) {
        *error = QStringLiteral("No files were selected");
        return false;
    }

    QList<CompressionEntry> entries;
    for (const QString &input : m_inputs) {
        const QFileInfo info(input);
        if (!collectCompressionEntries(input, info.fileName(), &entries, error))
            return false;
    }

    if (isCanceled())
        return false;

    const QString parentPath = QFileInfo(m_outputPath).absolutePath();
    if (!QDir().exists(parentPath)) {
        *error = QStringLiteral("The destination folder does not exist: %1").arg(parentPath);
        return false;
    }

    const QString tempPath = temporaryPath(parentPath, QStringLiteral("cutefish-archive"));
    int openError = 0;
    const QByteArray tempPathBytes = QFile::encodeName(tempPath);
    zip_t *archive = zip_open(tempPathBytes.constData(), ZIP_CREATE | ZIP_TRUNCATE, &openError);
    if (!archive) {
        QFile::remove(tempPath);
        *error = zipOpenError(openError);
        return false;
    }

    if (zip_register_progress_callback_with_state(archive, 0.01, &ArchiveJob::zipProgressCallback,
                                                  nullptr, this) != 0 ||
        zip_register_cancel_callback_with_state(archive, &ArchiveJob::zipCancelCallback,
                                                nullptr, this) != 0) {
        *error = zipError(archive);
        zip_discard(archive);
        QFile::remove(tempPath);
        return false;
    }

    emit progressChanged(0);
    for (const CompressionEntry &entry : entries) {
        if (isCanceled()) {
            zip_discard(archive);
            QFile::remove(tempPath);
            return false;
        }

        // Keep the progress UI focused on the selected item. A folder may
        // contain many entries, but its internal archive paths are not useful
        // to show while the archive is being created.
        emit currentFileChanged(topLevelArchiveName(entry.archivePath));

        const QByteArray archiveName = entry.archivePath.toUtf8();
        if (entry.directory) {
            if (zip_dir_add(archive, archiveName.constData(), ZIP_FL_ENC_UTF_8) < 0) {
                *error = zipError(archive);
                zip_discard(archive);
                QFile::remove(tempPath);
                return false;
            }
            continue;
        }

        zip_source_t *source = nullptr;
        if (entry.symbolicLink) {
            const QByteArray linkTarget = entry.linkTarget.toUtf8();
            const size_t bufferSize = qMax<size_t>(linkTarget.size(), 1);
            char *buffer = static_cast<char *>(std::malloc(bufferSize));
            if (!buffer) {
                *error = QStringLiteral("Could not allocate memory for symbolic link");
                zip_discard(archive);
                QFile::remove(tempPath);
                return false;
            }

            std::memcpy(buffer, linkTarget.constData(), static_cast<size_t>(linkTarget.size()));
            source = zip_source_buffer(archive, buffer, linkTarget.size(), 1);
            if (!source)
                std::free(buffer);
        } else {
            const QByteArray sourceName = QFile::encodeName(entry.sourcePath);
            source = zip_source_file(archive, sourceName.constData(), 0, -1);
        }

        if (!source) {
            *error = zipError(archive);
            zip_discard(archive);
            QFile::remove(tempPath);
            return false;
        }

        const zip_int64_t entryIndex = zip_file_add(archive, archiveName.constData(), source,
                                                    ZIP_FL_ENC_UTF_8);
        if (entryIndex < 0) {
            zip_source_free(source);
            *error = zipError(archive);
            zip_discard(archive);
            QFile::remove(tempPath);
            return false;
        }

        // Use a fast, low-level DEFLATE profile. This keeps compression close
        // to Finder's quick archive behavior while avoiding an unnecessarily
        // expensive maximum-compression pass.
        if (zip_set_file_compression(archive,
                                     static_cast<zip_uint64_t>(entryIndex),
                                     ZIP_CM_DEFLATE,
                                     1) != 0) {
            *error = zipError(archive);
            zip_discard(archive);
            QFile::remove(tempPath);
            return false;
        }

        if (entry.symbolicLink &&
            zip_file_set_external_attributes(archive,
                                             static_cast<zip_uint64_t>(entryIndex),
                                             0,
                                             ZIP_OPSYS_UNIX,
                                             static_cast<zip_uint32_t>(S_IFLNK | 0777) << 16) != 0) {
            *error = zipError(archive);
            zip_discard(archive);
            QFile::remove(tempPath);
            return false;
        }

    }

    if (isCanceled()) {
        zip_discard(archive);
        QFile::remove(tempPath);
        return false;
    }

    if (zip_close(archive) != 0) {
        *error = zipError(archive);
        zip_discard(archive);
        QFile::remove(tempPath);
        return false;
    }

    if (isCanceled() || !QFile::rename(tempPath, m_outputPath)) {
        QFile::remove(tempPath);
        if (!isCanceled())
            *error = QStringLiteral("Could not create %1").arg(m_outputPath);
        return false;
    }

    emit progressChanged(100);
    return true;
}

bool ArchiveJob::archiveEntryPath(const QString &archiveName,
                                  QString *relativePath,
                                  bool *directory) const
{
    QString name = archiveName;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));

    *directory = isDirectoryEntry(name);
    while (name.endsWith(QLatin1Char('/')))
        name.chop(1);

    if (name.isEmpty()) {
        relativePath->clear();
        return true;
    }

    if (name.startsWith(QLatin1Char('/')) ||
        (name.size() >= 2 && name.at(1) == QLatin1Char(':')) ||
        name.contains(QChar::Null))
        return false;

    const QStringList components = name.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &component : components) {
        if (component == QLatin1String(".."))
            return false;
    }

    const QString cleanName = QDir::cleanPath(name);
    // Tar archives made from "." begin with a "./" entry for the root.
    if (cleanName == QLatin1String(".")) {
        relativePath->clear();
        return true;
    }

    if (cleanName.startsWith(QLatin1String("../")) ||
        cleanName == QLatin1String("..") || cleanName.startsWith(QLatin1Char('/'))) {
        return false;
    }

    *relativePath = cleanName;
    return true;
}

bool ArchiveJob::extractEntryData(struct archive *reader, int fd, quint64 total, QString *error)
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
            const ssize_t result = ::write(fd, buffer + written,
                                           static_cast<size_t>(bytesRead - written));
            if (result < 0) {
                if (errno == EINTR)
                    continue;

                *error = qt_error_string(errno);
                return false;
            }
            written += result;
        }

        emitProgress(static_cast<quint64>(qMax<la_int64_t>(0, archive_filter_bytes(reader, -1))),
                     total);
    }
}

bool ArchiveJob::extractSingleFile(struct archive *reader, bool empty,
                                   const QString &parentPath, quint64 total, QString *error)
{
    const QString tempPath = temporaryPath(parentPath, QStringLiteral("cutefish-extract"));
    const QByteArray tempName = QFile::encodeName(tempPath);
    const int fd = ::open(tempName.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0) {
        *error = QStringLiteral("Could not create %1: %2")
                         .arg(QFileInfo(m_outputPath).fileName(), qt_error_string(errno));
        return false;
    }

    auto cleanup = qScopeGuard([&tempPath] { QFile::remove(tempPath); });
    emit currentFileChanged(QFileInfo(m_outputPath).fileName());

    const bool written = empty || extractEntryData(reader, fd, total, error);
    if (::close(fd) != 0 && written) {
        *error = qt_error_string(errno);
        return false;
    }

    if (!written)
        return false;

    if (isCanceled() || !QFile::rename(tempPath, m_outputPath)) {
        if (!isCanceled())
            *error = QStringLiteral("Could not create %1").arg(m_outputPath);
        return false;
    }

    cleanup.dismiss();
    emit progressChanged(100);
    return true;
}

bool ArchiveJob::extract(QString *error)
{
    if (m_inputs.size() != 1) {
        *error = QStringLiteral("Only one archive can be extracted at a time");
        return false;
    }

    const QString archivePath = m_inputs.first();
    if (isArchiveVolume(archivePath)) {
        *error = QStringLiteral("Multi-part archives are not supported");
        return false;
    }

    const QString parentPath = QFileInfo(m_outputPath).absolutePath();
    if (!QDir().exists(parentPath)) {
        *error = QStringLiteral("The destination folder does not exist: %1").arg(parentPath);
        return false;
    }

    ArchiveReader reader = openArchiveReader(archivePath, error);
    if (!reader)
        return false;

    // A compressed stream cannot be sized without decompressing it, so
    // progress follows how much of the archive file has been read.
    const quint64 total = static_cast<quint64>(qMax<qint64>(0, QFileInfo(archivePath).size()));
    emit progressChanged(0);

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
            *error = QStringLiteral("Unrecognized archive format");
            return false;
        }

        return extractSingleFile(reader.get(), status == ARCHIVE_EOF, parentPath, total, error);
    }

    const QString tempPath = temporaryPath(parentPath, QStringLiteral("cutefish-extract"));
    if (!QDir().mkpath(tempPath)) {
        *error = QStringLiteral("Could not create a temporary extraction folder");
        return false;
    }

    auto cleanup = qScopeGuard([&tempPath] { QDir(tempPath).removeRecursively(); });

    struct PendingLink {
        QString relativePath;
        QString target;
    };
    struct PendingDirectory {
        QByteArray path;
        struct timespec modified;
    };
    QList<PendingLink> links;
    QList<PendingDirectory> directories;

    for (; status != ARCHIVE_EOF; status = archive_read_next_header(reader.get(), &entry)) {
        if (status < ARCHIVE_WARN) {
            *error = archiveError(reader.get());
            return false;
        }

        if (isCanceled())
            return false;

        const QString name = entryPathName(entry);
        QString relativePath;
        bool directory = false;
        if (!archiveEntryPath(name, &relativePath, &directory)) {
            *error = QStringLiteral("Unsafe path in archive: %1").arg(name);
            return false;
        }

        if (archive_entry_is_encrypted(entry)) {
            *error = QStringLiteral("Password-protected archives are not supported");
            return false;
        }

        if (archive_entry_hardlink(entry)) {
            *error = QStringLiteral("Hard links in archives are not supported: %1").arg(name);
            return false;
        }

        const mode_t type = archive_entry_filetype(entry);
        if (type == AE_IFLNK) {
            const QString target = entryLinkTarget(entry);
            if (directory || relativePath.isEmpty()) {
                *error = QStringLiteral("Invalid symbolic link path in archive: %1").arg(name);
                return false;
            }

            if (target.isEmpty() || target.size() > 16 * 1024) {
                *error = QStringLiteral("Invalid symbolic link target: %1").arg(relativePath);
                return false;
            }

            links.append({relativePath, target});
            continue;
        }

        if (type != AE_IFREG && type != AE_IFDIR) {
            *error = QStringLiteral("Special files in archives are not supported: %1").arg(name);
            return false;
        }

        if (relativePath.isEmpty())
            continue;

        const QString destinationPath = QDir(tempPath).filePath(relativePath);
        const QByteArray destinationName = QFile::encodeName(destinationPath);
        // Set-user-ID and set-group-ID bits never survive extraction.
        const mode_t permissions = archive_entry_perm(entry) & 0777;
        emit currentFileChanged(relativePath);

        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())) {
            *error = QStringLiteral("Could not create the parent folder for %1").arg(relativePath);
            return false;
        }

        if (type == AE_IFDIR || directory) {
            // A folder first met as a parent keeps the default mode.
            if ((::mkdir(destinationName.constData(), permissions | S_IRWXU) != 0 && errno != EEXIST)
                || !QFileInfo(destinationPath).isDir()) {
                *error = QStringLiteral("Could not create %1").arg(relativePath);
                return false;
            }

            if (archive_entry_mtime_is_set(entry))
                directories.append({destinationName, entryModified(entry)});
            continue;
        }

        const int fd = ::open(destinationName.constData(),
                              O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
                              permissions | S_IRUSR | S_IWUSR);
        if (fd < 0) {
            *error = QStringLiteral("Could not create %1: %2").arg(relativePath, qt_error_string(errno));
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
            *error = QStringLiteral("Could not write %1: %2").arg(relativePath, qt_error_string(errno));
            return false;
        }
    }

    // Create symbolic links only after all regular files have been written.
    // A link inside an archive must never redirect extraction of a later file.
    for (const PendingLink &link : std::as_const(links)) {
        if (isCanceled())
            return false;

        emit currentFileChanged(link.relativePath);

        const QString destinationPath = QDir(tempPath).filePath(link.relativePath);
        if (hasSymbolicLinkParent(tempPath, link.relativePath)) {
            *error = QStringLiteral("Symbolic link parent in archive: %1").arg(link.relativePath);
            return false;
        }

        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())) {
            *error = QStringLiteral("Could not create the parent folder for %1").arg(link.relativePath);
            return false;
        }

        const QFileInfo existing(destinationPath);
        if (existing.exists() || existing.isSymLink()) {
            *error = QStringLiteral("Duplicate path in archive: %1").arg(link.relativePath);
            return false;
        }

        if (!QFile::link(link.target, destinationPath)) {
            *error = QStringLiteral("Could not restore symbolic link: %1").arg(link.relativePath);
            return false;
        }
    }

    // Writing into a folder changes its time, so folders get theirs last.
    for (const PendingDirectory &directory : std::as_const(directories)) {
        const struct timespec times[2] = {{0, UTIME_OMIT}, directory.modified};
        ::utimensat(AT_FDCWD, directory.path.constData(), times, AT_SYMLINK_NOFOLLOW);
    }

    if (isCanceled() || !QFile::rename(tempPath, m_outputPath)) {
        if (!isCanceled())
            *error = QStringLiteral("Could not create %1").arg(m_outputPath);
        return false;
    }

    cleanup.dismiss();
    emit progressChanged(100);
    return true;
}

void ArchiveJob::zipProgressCallback(zip_t *archive, double progress, void *state)
{
    Q_UNUSED(archive)

    auto *job = static_cast<ArchiveJob *>(state);
    if (!job)
        return;

    job->emitProgress(static_cast<quint64>(progress * 100.0), 100);
}

int ArchiveJob::zipCancelCallback(zip_t *archive, void *state)
{
    Q_UNUSED(archive)

    auto *job = static_cast<ArchiveJob *>(state);
    return job && job->isCanceled() ? 1 : 0;
}
