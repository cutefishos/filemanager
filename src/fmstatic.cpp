#include "fmstatic.h"

#include <QDesktopServices>

#include <KRun>
#include <KCoreDirLister>
#include <KFileItem>
#include <KFilePlacesModel>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/EmptyTrashJob>
#include <KIO/MkdirJob>
#include <KIO/SimpleJob>
#include <QIcon>

FMStatic::FMStatic(QObject *parent)
    : QObject(parent)
{
}

FMH::MODEL_LIST FMStatic::packItems(const QStringList &items, const QString &type)
{
    FMH::MODEL_LIST data;

    for (const auto &path : items) {
        if (QUrl(path).isLocalFile() && !FMH::fileExists(path))
            continue;

        auto model = FMH::getFileInfoModel(path);
        model.insert(FMH::MODEL_KEY::TYPE, type);
        data << model;
    }

    return data;
}

FMH::MODEL_LIST FMStatic::getDefaultPaths()
{
    return FMStatic::packItems(FMH::defaultPaths, FMH::PATHTYPE_LABEL[FMH::PATHTYPE_KEY::PLACES_PATH]);
}

FMH::MODEL_LIST FMStatic::search(const QString &query, const QUrl &path, const bool &hidden, const bool &onlyDirs, const QStringList &filters)
{
    FMH::MODEL_LIST content;

    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file. FM::search" << path;
        return content;
    }

    if (FMStatic::isDir(path)) {
        QDir::Filters dirFilter;

        dirFilter = (onlyDirs ? QDir::AllDirs | QDir::NoDotDot | QDir::NoDot : QDir::Files | QDir::AllDirs | QDir::NoDotDot | QDir::NoDot);

        if (hidden)
            dirFilter = dirFilter | QDir::Hidden | QDir::System;

        QDirIterator it(path.toLocalFile(), filters, dirFilter, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            auto url = it.next();
            if (it.fileName().contains(query, Qt::CaseInsensitive)) {
                content << FMH::getFileInfoModel(QUrl::fromLocalFile(url));
            }
        }
    } else
        qWarning() << "Search path does not exists" << path;

    qDebug() << content;
    return content;
}

FMH::MODEL_LIST FMStatic::getDevices()
{
    FMH::MODEL_LIST drives;

    return drives;
}

QVariantMap FMStatic::getDirInfo(const QUrl &path)
{
    return FMH::getDirInfo(path);
}

QVariantMap FMStatic::getFileInfo(const QUrl &path)
{
    return FMH::getFileInfo(path);
}

bool FMStatic::isDefaultPath(const QString &path)
{
    return FMH::defaultPaths.contains(path);
}

QUrl FMStatic::parentDir(const QUrl &path)
{
    return FMH::parentDir(path);
}

bool FMStatic::isDir(const QUrl &path)
{
    if (!path.isLocalFile()) {
        //         qWarning() << "URL recived is not a local file. FM::isDir" << path;
        return false;
    }

    const QFileInfo file(path.toLocalFile());
    return file.isDir();
}

bool FMStatic::isCloud(const QUrl &path)
{
    return path.scheme() == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::CLOUD_PATH];
}

bool FMStatic::fileExists(const QUrl &path)
{
    return FMH::fileExists(path);
}

QString FMStatic::fileDir(const QUrl &path) // the directory path of the file
{
    return FMH::fileDir(path);
}

QString FMStatic::formatSize(const int &size)
{
    const QLocale locale;
    return locale.formattedDataSize(size);
}

QString FMStatic::formatDate(const QString &dateStr, const QString &format, const QString &initFormat)
{
    if (initFormat.isEmpty())
        return QDateTime::fromString(dateStr, Qt::TextDate).toString(format);
    else
        return QDateTime::fromString(dateStr, initFormat).toString(format);
}

QString FMStatic::systemFormatDate(const QString &dateStr)
{
    return QLocale::system().toString(QDateTime::fromString(dateStr, Qt::TextDate),
                                      QLocale::ShortFormat);
}

QString FMStatic::formatTime(const qint64 &value)
{
    QString tStr;
    if (value) {
        QTime time((value / 3600) % 60, (value / 60) % 60, value % 60, (value * 1000) % 1000);
        QString format = "mm:ss";
        if (value > 3600)
            format = "hh:mm:ss";
        tStr = time.toString(format);
    }

    return tStr.isEmpty() ? "00:00" : tStr;
}

QString FMStatic::homePath()
{
    return FMH::HomePath;
}

bool FMStatic::copy(const QList<QUrl> &urls, const QUrl &destinationDir)
{
    auto job = KIO::copy(urls, destinationDir);
    job->start();
    return true;
}

bool FMStatic::cut(const QList<QUrl> &urls, const QUrl &where)
{
    return FMStatic::cut(urls, where, QString());
}

bool FMStatic::cut(const QList<QUrl> &urls, const QUrl &where, const QString &name)
{
    QUrl _where = where;
    if (!name.isEmpty())
        _where = QUrl(where.toString() + "/" + name);

    auto job = KIO::move(urls, _where, KIO::HideProgressInfo);
    job->start();
    return true;
}

bool FMStatic::removeFiles(const QList<QUrl> &urls)
{
    auto job = KIO::del(urls);
    job->start();
    return true;
}

void FMStatic::moveToTrash(const QList<QUrl> &urls)
{
    auto job = KIO::trash(urls);
    job->start();
}

void FMStatic::emptyTrash()
{
    auto job = KIO::emptyTrash();
    job->start();
}

bool FMStatic::removeDir(const QUrl &path)
{
    bool result = true;
    QDir dir(path.toLocalFile());
    qDebug() << "TRYING TO REMOVE DIR" << path << path.toLocalFile();
    if (dir.exists()) {
        Q_FOREACH (QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) {
                result = removeDir(QUrl::fromLocalFile(info.absoluteFilePath()));
            } else {
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(path.toLocalFile());
    }

    return result;
}

bool FMStatic::rename(const QUrl &url, const QString &name)
{
    return FMStatic::cut({url}, QUrl(url.toString().left(url.toString().lastIndexOf("/"))), name);
}

bool FMStatic::createDir(const QUrl &path, const QString &name)
{
    auto job = KIO::mkdir(name.isEmpty() ? path : QUrl(path.toString() + "/" + name));
    job->start();
    return true;
}

bool FMStatic::createFile(const QUrl &path, const QString &name)
{
    QFile file(path.toLocalFile() + "/" + name);

    if (file.open(QIODevice::ReadWrite)) {
        file.close();
        return true;
    }

    return false;
}

bool FMStatic::createSymlink(const QUrl &path, const QUrl &where)
{
    qDebug() << "trying to create symlink" << path << where;
    const auto job = KIO::link({path}, where);
    job->start();
    return true;
}

bool FMStatic::openUrl(const QUrl &url)
{
    KRun::runUrl(url, FMH::getFileInfoModel(url)[FMH::MODEL_KEY::MIME], nullptr, false, KRun::RunFlag::DeleteTemporaryFiles);
    return true;
}

void FMStatic::openLocation(const QStringList &urls)
{
    for (const auto &url : qAsConst(urls))
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(url).dir().absolutePath()));
}

const QVariantMap FMStatic::dirConf(const QUrl &path)
{
    return FMH::dirConf(path);
}

void FMStatic::setDirConf(const QUrl &path, const QString &group, const QString &key, const QVariant &value)
{
    FMH::setDirConf(path, group, key, value);
}

bool FMStatic::checkFileType(const int &type, const QString &mimeTypeName)
{
    return FMH::checkFileType(static_cast<FMH::FILTER_TYPE>(type), mimeTypeName);
}

static bool doNameFilter(const QString &name, const QStringList &filters)
{
    const auto filtersAccumulate = std::accumulate(filters.constBegin(), filters.constEnd(), QVector<QRegExp> {}, [](QVector<QRegExp> &res, const QString &filter) -> QVector<QRegExp> {
        res.append(QRegExp(filter, Qt::CaseInsensitive, QRegExp::Wildcard));
        return res;
    });

    for (const auto &filter : filtersAccumulate) {
        if (filter.exactMatch(name)) {
            return true;
        }
    }
    return false;
}

QStringList FMStatic::nameFilters(const int &type)
{
    return FMH::FILTER_LIST[static_cast<FMH::FILTER_TYPE>(type)];
}

QString FMStatic::iconName(const QString &value)
{
    return FMH::getIconName(value);
}
