#include "archivejob.h"

#include <zip.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QUuid>

#include <cstdlib>
#include <cstring>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {

QString zipError(zip_t *archive)
{
    if (!archive)
        return QStringLiteral("Unknown ZIP error");

    return QString::fromUtf8(zip_strerror(archive));
}

QString zipOpenError(int errorCode)
{
    zip_error_t error;
    zip_error_init_with_code(&error, errorCode);
    const QString message = QString::fromUtf8(zip_error_strerror(&error));
    zip_error_fini(&error);
    return message;
}

QString temporaryPath(const QString &parentPath, const QString &prefix)
{
    return QDir(parentPath).filePath(QStringLiteral(".%1-%2")
                                         .arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

bool isDirectoryEntry(const QString &name)
{
    return name.endsWith(QLatin1Char('/'));
}

bool hasSymbolicLinkParent(const QString &rootPath, const QString &relativePath)
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

QString topLevelArchiveName(const QString &archivePath)
{
    const qsizetype separator = archivePath.indexOf(QLatin1Char('/'));
    return separator < 0 ? archivePath : archivePath.left(separator);
}

} // namespace

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
    emit progressChanged(qBound(0, static_cast<int>(ratio * 100.0), 100));
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
    if (cleanName == QLatin1String(".") || cleanName.startsWith(QLatin1String("../")) ||
        cleanName == QLatin1String("..") || cleanName.startsWith(QLatin1Char('/'))) {
        return false;
    }

    *relativePath = cleanName;
    return true;
}

bool ArchiveJob::zipEntryIsSymbolicLink(zip_t *archive, zip_uint64_t index)
{
#ifdef Q_OS_UNIX
    zip_uint8_t operatingSystem = ZIP_OPSYS_UNIX;
    zip_uint32_t externalAttributes = 0;
    if (zip_file_get_external_attributes(archive, index, 0,
                                         &operatingSystem, &externalAttributes) != 0) {
        return false;
    }

    if (operatingSystem != ZIP_OPSYS_UNIX && operatingSystem != ZIP_OPSYS_OS_X)
        return false;

    const mode_t mode = static_cast<mode_t>(externalAttributes >> 16);
    return S_ISLNK(mode);
#else
    Q_UNUSED(archive)
    Q_UNUSED(index)
    return false;
#endif
}

bool ArchiveJob::extract(QString *error)
{
    if (m_inputs.size() != 1) {
        *error = QStringLiteral("Only one archive can be extracted at a time");
        return false;
    }

    const QString archivePath = m_inputs.first();
    const QByteArray archivePathBytes = QFile::encodeName(archivePath);
    int openError = 0;
    zip_t *archive = zip_open(archivePathBytes.constData(), 0, &openError);
    if (!archive) {
        *error = zipOpenError(openError);
        return false;
    }

    quint64 total = 0;
    const zip_int64_t entriesCount = zip_get_num_entries(archive, 0);
    if (entriesCount < 0) {
        *error = zipError(archive);
        zip_discard(archive);
        return false;
    }

    for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(entriesCount); ++index) {
        if (isCanceled()) {
            zip_discard(archive);
            return false;
        }

        zip_stat_t stat;
        zip_stat_init(&stat);
        if (zip_stat_index(archive, index, 0, &stat) != 0 || !stat.name) {
            *error = zipError(archive);
            zip_discard(archive);
            return false;
        }

        QString relativePath;
        bool directory = false;
        if (!archiveEntryPath(QString::fromUtf8(stat.name), &relativePath, &directory)) {
            *error = QStringLiteral("Unsafe path in archive: %1").arg(QString::fromUtf8(stat.name));
            zip_discard(archive);
            return false;
        }

        if (!directory && !zipEntryIsSymbolicLink(archive, index))
            total += stat.size;
    }

    const QString parentPath = QFileInfo(m_outputPath).absolutePath();
    if (!QDir().exists(parentPath)) {
        *error = QStringLiteral("The destination folder does not exist: %1").arg(parentPath);
        zip_discard(archive);
        return false;
    }

    const QString tempPath = temporaryPath(parentPath, QStringLiteral("cutefish-extract"));
    if (!QDir().mkpath(tempPath)) {
        *error = QStringLiteral("Could not create a temporary extraction folder");
        zip_discard(archive);
        return false;
    }

    quint64 processed = 0;
    emit progressChanged(0);

    for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(entriesCount); ++index) {
        if (isCanceled()) {
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        zip_stat_t stat;
        zip_stat_init(&stat);
        if (zip_stat_index(archive, index, 0, &stat) != 0 || !stat.name) {
            *error = zipError(archive);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        QString relativePath;
        bool directory = false;
        if (!archiveEntryPath(QString::fromUtf8(stat.name), &relativePath, &directory)) {
            *error = QStringLiteral("Unsafe path in archive: %1").arg(QString::fromUtf8(stat.name));
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        if (zipEntryIsSymbolicLink(archive, index))
            continue;

        if (relativePath.isEmpty())
            continue;

        const QString destinationPath = QDir(tempPath).filePath(relativePath);
        emit currentFileChanged(relativePath);

        if (directory) {
            if (!QDir().mkpath(destinationPath)) {
                *error = QStringLiteral("Could not create %1").arg(relativePath);
                zip_discard(archive);
                QDir(tempPath).removeRecursively();
                return false;
            }
            continue;
        }

        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())) {
            *error = QStringLiteral("Could not create the parent folder for %1").arg(relativePath);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        zip_file_t *file = zip_fopen_index(archive, index, 0);
        if (!file) {
            *error = zipError(archive);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        QFile output(destinationPath);
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            *error = output.errorString();
            zip_fclose(file);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        char buffer[64 * 1024];
        while (true) {
            if (isCanceled()) {
                output.close();
                zip_fclose(file);
                zip_discard(archive);
                QDir(tempPath).removeRecursively();
                return false;
            }

            const zip_int64_t bytesRead = zip_fread(file, buffer, sizeof(buffer));
            if (bytesRead < 0) {
                *error = zipError(archive);
                output.close();
                zip_fclose(file);
                zip_discard(archive);
                QDir(tempPath).removeRecursively();
                return false;
            }

            if (bytesRead == 0)
                break;

            if (output.write(buffer, bytesRead) != bytesRead) {
                *error = output.errorString();
                output.close();
                zip_fclose(file);
                zip_discard(archive);
                QDir(tempPath).removeRecursively();
                return false;
            }

            processed += static_cast<quint64>(bytesRead);
            emitProgress(processed, total);
        }

        output.close();
        if (zip_fclose(file) != 0) {
            *error = zipError(archive);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }
    }

    // Create symbolic links only after all regular files have been written.
    // A link inside an archive must never redirect extraction of a later file.
    for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(entriesCount); ++index) {
        if (isCanceled()) {
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        if (!zipEntryIsSymbolicLink(archive, index))
            continue;

        zip_stat_t stat;
        zip_stat_init(&stat);
        if (zip_stat_index(archive, index, 0, &stat) != 0 || !stat.name) {
            *error = zipError(archive);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        QString relativePath;
        bool directory = false;
        if (!archiveEntryPath(QString::fromUtf8(stat.name), &relativePath, &directory) ||
            directory || relativePath.isEmpty()) {
            *error = QStringLiteral("Invalid symbolic link path in archive: %1")
                         .arg(QString::fromUtf8(stat.name));
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        emit currentFileChanged(relativePath);

        const QString destinationPath = QDir(tempPath).filePath(relativePath);
        if (hasSymbolicLinkParent(tempPath, relativePath)) {
            *error = QStringLiteral("Symbolic link parent in archive: %1").arg(relativePath);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())) {
            *error = QStringLiteral("Could not create the parent folder for %1").arg(relativePath);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        const QFileInfo existing(destinationPath);
        if (existing.exists() || existing.isSymLink()) {
            *error = QStringLiteral("Duplicate path in archive: %1").arg(relativePath);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        zip_file_t *file = zip_fopen_index(archive, index, 0);
        if (!file) {
            *error = zipError(archive);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

        QByteArray target;
        char buffer[4096];
        bool readError = false;
        while (true) {
            const zip_int64_t bytesRead = zip_fread(file, buffer, sizeof(buffer));
            if (bytesRead < 0) {
                readError = true;
                break;
            }
            if (bytesRead == 0)
                break;

            if (target.size() > 16 * 1024 - bytesRead) {
                *error = QStringLiteral("Symbolic link target is too long: %1").arg(relativePath);
                readError = true;
                break;
            }
            target.append(buffer, static_cast<qsizetype>(bytesRead));
        }

        const int closeResult = zip_fclose(file);
        if (readError || closeResult != 0 || target.contains('\0')) {
            if (readError && error->isEmpty())
                *error = zipError(archive);
            if (error->isEmpty())
                *error = QStringLiteral("Invalid symbolic link target: %1").arg(relativePath);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }

#ifdef Q_OS_UNIX
        if (!QFile::link(QString::fromUtf8(target), destinationPath)) {
            *error = QStringLiteral("Could not restore symbolic link: %1").arg(relativePath);
            zip_discard(archive);
            QDir(tempPath).removeRecursively();
            return false;
        }
#else
        *error = QStringLiteral("Symbolic links are not supported on this platform");
        zip_discard(archive);
        QDir(tempPath).removeRecursively();
        return false;
#endif
    }

    zip_discard(archive);

    if (isCanceled() || !QFile::rename(tempPath, m_outputPath)) {
        QDir(tempPath).removeRecursively();
        if (!isCanceled())
            *error = QStringLiteral("Could not create %1").arg(m_outputPath);
        return false;
    }

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
