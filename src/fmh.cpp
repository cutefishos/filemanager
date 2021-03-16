#include "fmh.h"

namespace FMH
{
const QVector<int> modelRoles(const FMH::MODEL &model)
{
    const auto keys = model.keys();
    return std::accumulate(keys.begin(), keys.end(), QVector<int>(), [](QVector<int> &res, const FMH::MODEL_KEY &key) {
        res.append(key);
        return res;
    });
}

const QString mapValue(const QVariantMap &map, const FMH::MODEL_KEY &key)
{
    return map[FMH::MODEL_NAME[key]].toString();
}

const QVariantMap toMap(const FMH::MODEL &model)
{
    QVariantMap map;
    for (const auto &key : model.keys())
        map.insert(FMH::MODEL_NAME[key], model[key]);

    return map;
}

const FMH::MODEL toModel(const QVariantMap &map)
{
    FMH::MODEL model;
    for (const auto &key : map.keys())
        model.insert(FMH::MODEL_NAME_KEY[key], map[key].toString());

    return model;
}

const FMH::MODEL_LIST toModelList(const QVariantList &list)
{
    FMH::MODEL_LIST res;
    return std::accumulate(list.constBegin(), list.constEnd(), res, [](FMH::MODEL_LIST &res, const QVariant &item) -> FMH::MODEL_LIST {
        res << FMH::toModel(item.toMap());
        return res;
    });
}

const QVariantList toMapList(const FMH::MODEL_LIST &list)
{
    QVariantList res;
    return std::accumulate(list.constBegin(), list.constEnd(), res, [](QVariantList &res, const FMH::MODEL &item) -> QVariantList {
        res << FMH::toMap(item);
        return res;
    });
}

const FMH::MODEL filterModel(const FMH::MODEL &model, const QVector<FMH::MODEL_KEY> &keys)
{
    FMH::MODEL res;
    return std::accumulate(keys.constBegin(), keys.constEnd(), res, [=](FMH::MODEL &res, const FMH::MODEL_KEY &key) -> FMH::MODEL {
        if (model.contains(key))
            res[key] = model[key];
        return res;
    });
}

const QStringList modelToList(const FMH::MODEL_LIST &list, const FMH::MODEL_KEY &key)
{
    QStringList res;
    return std::accumulate(list.constBegin(), list.constEnd(), res, [key](QStringList &res, const FMH::MODEL &item) -> QStringList {
        if (item.contains(key))
            res << item[key];
        return res;
    });
}

bool fileExists(const QUrl &path)
{
    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file" << path;
        return false;
    }
    return QFileInfo::exists(path.toLocalFile());
}

const QString fileDir(const QUrl &path) // the directory path of the file
{
    QString res = path.toString();
    if (path.isLocalFile()) {
        const QFileInfo file(path.toLocalFile());
        if (file.isDir())
            res = path.toString();
        else
            res = QUrl::fromLocalFile(file.dir().absolutePath()).toString();
    } else
        qWarning() << "The path is not a local one. FM::fileDir";

    return res;
}

const QUrl parentDir(const QUrl &path)
{
    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file, FM::parentDir" << path;
        return path;
    }

    QDir dir(path.toLocalFile());
    dir.cdUp();
    return QUrl::fromLocalFile(dir.absolutePath());
}

const QVariantMap dirConf(const QUrl &path)
{
    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file" << path;
        return QVariantMap();
    }

    if (!fileExists(path))
        return QVariantMap();

    QString icon, iconsize, hidden, detailview, showthumbnail, showterminal;

    uint count = 0, sortby = MODEL_KEY::MODIFIED, viewType = 0;

    bool foldersFirst = false;

#if defined Q_OS_ANDROID || defined Q_OS_WIN || defined Q_OS_MACOS || defined Q_OS_IOS
    QSettings file(path.toLocalFile(), QSettings::Format::NativeFormat);
    file.beginGroup(QString("Desktop Entry"));
    icon = file.value("Icon").toString();
    file.endGroup();

    file.beginGroup(QString("Settings"));
    hidden = file.value("HiddenFilesShown").toString();
    file.endGroup();

    file.beginGroup(QString("MAUIFM"));
    iconsize = file.value("IconSize").toString();
    detailview = file.value("DetailView").toString();
    showthumbnail = file.value("ShowThumbnail").toString();
    showterminal = file.value("ShowTerminal").toString();
    count = file.value("Count").toInt();
    sortby = file.value("SortBy").toInt();
    foldersFirst = file.value("FoldersFirst").toBool();
    viewType = file.value("ViewType").toInt();
    file.endGroup();

#else
    KConfig file(path.toLocalFile());
    icon = file.entryMap(QString("Desktop Entry"))["Icon"];
    hidden = file.entryMap(QString("Settings"))["HiddenFilesShown"];
    iconsize = file.entryMap(QString("MAUIFM"))["IconSize"];
    detailview = file.entryMap(QString("MAUIFM"))["DetailView"];
    showthumbnail = file.entryMap(QString("MAUIFM"))["ShowThumbnail"];
    showterminal = file.entryMap(QString("MAUIFM"))["ShowTerminal"];
    count = file.entryMap(QString("MAUIFM"))["Count"].toInt();
    sortby = file.entryMap(QString("MAUIFM"))["SortBy"].toInt();
    foldersFirst = file.entryMap(QString("MAUIFM"))["FoldersFirst"] == "true" ? true : false;
    viewType = file.entryMap(QString("MAUIFM"))["ViewType"].toInt();
#endif

    return QVariantMap({{MODEL_NAME[MODEL_KEY::ICON], icon.isEmpty() ? "folder" : icon},
                        {MODEL_NAME[MODEL_KEY::ICONSIZE], iconsize},
                        {MODEL_NAME[MODEL_KEY::COUNT], count},
                        {MODEL_NAME[MODEL_KEY::SHOWTERMINAL], showterminal.isEmpty() ? "false" : showterminal},
                        {MODEL_NAME[MODEL_KEY::SHOWTHUMBNAIL], showthumbnail.isEmpty() ? "false" : showthumbnail},
                        {MODEL_NAME[MODEL_KEY::DETAILVIEW], detailview.isEmpty() ? "false" : detailview},
                        {MODEL_NAME[MODEL_KEY::HIDDEN], hidden.isEmpty() ? false : (hidden == "true" ? true : false)},
                        {MODEL_NAME[MODEL_KEY::SORTBY], sortby},
                        {MODEL_NAME[MODEL_KEY::FOLDERSFIRST], foldersFirst},
                        {MODEL_NAME[MODEL_KEY::VIEWTYPE], viewType}});
}

void setDirConf(const QUrl &path, const QString &group, const QString &key, const QVariant &value)
{
    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file" << path;
        return;
    }

#if defined Q_OS_ANDROID || defined Q_OS_WIN || defined Q_OS_MACOS || defined Q_OS_IOS
    QSettings file(path.toLocalFile(), QSettings::Format::IniFormat);
    file.beginGroup(group);
    file.setValue(key, value);
    file.endGroup();
    file.sync();
#else
    KConfig file(path.toLocalFile(), KConfig::SimpleConfig);
    auto kgroup = file.group(group);
    kgroup.writeEntry(key, value);
    // 		file.reparseConfiguration();
    file.sync();
#endif
}

const QString getIconName(const QUrl &path)
{
    if (path.isLocalFile() && QFileInfo(path.toLocalFile()).isDir()) {
        if (folderIcon.contains(path.toString()))
            return folderIcon[path.toString()];
        else {
            const auto icon = dirConf(QString(path.toString() + "/%1").arg(".directory"))[MODEL_NAME[MODEL_KEY::ICON]].toString();
            return icon.isEmpty() ? "folder" : icon;
        }

    } else {
        QMimeDatabase mime;
        const auto type = mime.mimeTypeForFile(path.toString());
        return type.iconName();

//        KFileItem mime(path);
//        return mime.iconName();
    }
}

const QString getMime(const QUrl &path)
{
    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file, getMime" << path;
        return QString();
    }

    const QMimeDatabase mimedb;
    return mimedb.mimeTypeForFile(path.toLocalFile()).name();
}

const QUrl thumbnailUrl(const QUrl &url, const QString &mimetype)
{
#if defined Q_OS_LINUX && !defined Q_OS_ANDROID
    if (checkFileType(FILTER_TYPE::DOCUMENT, mimetype) || checkFileType(FILTER_TYPE::VIDEO, mimetype)) {
        return QUrl("image://thumbnailer/" + url.toString());
    }
#endif

    if (checkFileType(FILTER_TYPE::IMAGE, mimetype)) {
        return url;
    }

    return QUrl();
}

#if (!defined Q_OS_ANDROID && defined Q_OS_LINUX) || defined Q_OS_WIN
const FMH::MODEL getFileInfo(const KFileItem &kfile)
{
    return MODEL {{MODEL_KEY::LABEL, kfile.name()},
                  {MODEL_KEY::NAME, kfile.name().remove(kfile.name().lastIndexOf("."), kfile.name().size())},
                  {MODEL_KEY::DATE, kfile.time(KFileItem::FileTimes::CreationTime).toString(Qt::TextDate)},
                  {MODEL_KEY::MODIFIED, kfile.time(KFileItem::FileTimes::ModificationTime).toString(Qt::TextDate)},
                  {MODEL_KEY::LAST_READ, kfile.time(KFileItem::FileTimes::AccessTime).toString(Qt::TextDate)},
                  {MODEL_KEY::PATH, kfile.mostLocalUrl().toString()},
                  {MODEL_KEY::URL, kfile.mostLocalUrl().toString()},
                  {MODEL_KEY::THUMBNAIL, thumbnailUrl(kfile.mostLocalUrl(), kfile.mimetype()).toString()},
                  {MODEL_KEY::SYMLINK, kfile.linkDest()},
                  {MODEL_KEY::IS_SYMLINK, QVariant(kfile.isLink()).toString()},
                  {MODEL_KEY::HIDDEN, QVariant(kfile.isHidden()).toString()},
                  {MODEL_KEY::IS_DIR, QVariant(kfile.isDir()).toString()},
                  {MODEL_KEY::IS_FILE, QVariant(kfile.isFile()).toString()},
                  {MODEL_KEY::WRITABLE, QVariant(kfile.isWritable()).toString()},
                  {MODEL_KEY::READABLE, QVariant(kfile.isReadable()).toString()},
                  {MODEL_KEY::EXECUTABLE, QVariant(kfile.isDesktopFile()).toString()},
                  {MODEL_KEY::MIME, kfile.mimetype()},
                  {MODEL_KEY::GROUP, kfile.group()},
                  {MODEL_KEY::ICON, kfile.iconName()},
                  // for set wallpaper.
                  {MODEL_KEY::IMG, QVariant(kfile.mimetype().startsWith("image/")).toString()},
                  {MODEL_KEY::SIZE, QString::number(kfile.size())},
                  {MODEL_KEY::OWNER, kfile.user()},
                  {MODEL_KEY::COUNT, kfile.isLocalFile() && kfile.isDir() ? QString::number(QDir(kfile.localPath()).count()) : "0"}};
}
#endif

const FMH::MODEL getFileInfoModel(const QUrl &path)
{
    MODEL res;
#if defined Q_OS_ANDROID || defined Q_OS_MACOS || defined Q_OS_IOS || defined Q_OS_WIN
    const QFileInfo file(path.toLocalFile());
    if (!file.exists())
        return MODEL();

    const auto mime = getMime(path);
    res = MODEL {{MODEL_KEY::GROUP, file.group()},
                 {MODEL_KEY::OWNER, file.owner()},
                 {MODEL_KEY::SUFFIX, file.completeSuffix()},
                 {MODEL_KEY::LABEL, /*file.isDir() ? file.baseName() :*/ path == HomePath ? QStringLiteral("Home") : file.fileName()},
                 {MODEL_KEY::NAME, file.fileName()},
                 {MODEL_KEY::DATE, file.birthTime().toString(Qt::TextDate)},
                 {MODEL_KEY::MODIFIED, file.lastModified().toString(Qt::TextDate)},
                 {MODEL_KEY::LAST_READ, file.lastRead().toString(Qt::TextDate)},
                 {MODEL_KEY::MIME, mime},
                 {MODEL_KEY::SYMLINK, file.symLinkTarget()},
                 {MODEL_KEY::IS_SYMLINK, QVariant(file.isSymLink()).toString()},
                 {MODEL_KEY::IS_FILE, QVariant(file.isFile()).toString()},
                 {MODEL_KEY::HIDDEN, QVariant(file.isHidden()).toString()},
                 {MODEL_KEY::IS_DIR, QVariant(file.isDir()).toString()},
                 {MODEL_KEY::WRITABLE, QVariant(file.isWritable()).toString()},
                 {MODEL_KEY::READABLE, QVariant(file.isReadable()).toString()},
                 {MODEL_KEY::EXECUTABLE, QVariant(file.suffix().endsWith(".desktop")).toString()},
                 {MODEL_KEY::ICON, getIconName(path)},
                 {MODEL_KEY::SIZE, QString::number(file.size()) /*locale.formattedDataSize(file.size())*/},
                 {MODEL_KEY::PATH, path.toString()},
                 {MODEL_KEY::URL, path.toString()},
                 {MODEL_KEY::THUMBNAIL, thumbnailUrl(path, mime).toString()},
                 {MODEL_KEY::COUNT, file.isDir() ? QString::number(QDir(path.toLocalFile()).count()) : "0"}};
#else
    res = getFileInfo(KFileItem(path, KFileItem::MimeTypeDetermination::NormalMimeTypeDetermination));
#endif
    return res;
}

const QVariantMap getFileInfo(const QUrl &path)
{
    return toMap(getFileInfoModel(path));
}

const MODEL getDirInfoModel(const QUrl &path, const QString &type)
{
    auto res = getFileInfoModel(path);
    res[MODEL_KEY::TYPE] = type;
    return res;
}

const QVariantMap getDirInfo(const QUrl &path)
{
    return toMap(getDirInfoModel(path));
}

PATHTYPE_KEY getPathType(const QUrl &url)
{
    return PATHTYPE_SCHEME_NAME[url.scheme()];
}

bool checkFileType(const FMH::FILTER_TYPE &type, const QString &mimeTypeName)
{
    return SUPPORTED_MIMETYPES[type].contains(mimeTypeName);
}

}
