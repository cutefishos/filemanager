#ifndef FM_H
#define FM_H

#include <QHash>
#include <QObject>
#include <QStorageInfo>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "fmh.h"
#include "fmstatic.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
class KCoreDirLister;
#else
class QFileSystemWatcher;

namespace FMH
{
class FileLoader;
}
/**
 * @brief The QDirLister class
 * Placeholder for the KCoreDirLister for other system other than GNU Linux
 */
class QDirLister : public QObject
{
    Q_OBJECT
public:
    explicit QDirLister(QObject *parent = nullptr);

public slots:
    /**
     * @brief openUrl
     * @param url
     * @return
     */
    bool openUrl(const QUrl &url);

    /**
     * @brief setNameFilter
     * @param filters
     */
    void setNameFilter(QString filters);

    /**
     * @brief setDirOnlyMode
     * @param value
     */
    void setDirOnlyMode(bool value);

    /**
     * @brief setShowingDotFiles
     * @param value
     */
    void setShowingDotFiles(bool value);

signals:
    void itemsReady(FMH::MODEL_LIST items, QUrl url);
    void itemReady(FMH::MODEL item, QUrl url);
    void completed(QUrl url);
    void itemsAdded(FMH::MODEL_LIST items, QUrl url);
    void itemsDeleted(FMH::MODEL_LIST items, QUrl url);
    void newItems(FMH::MODEL_LIST items, QUrl url);
    void refreshItems(QVector<QPair<FMH::MODEL, FMH::MODEL>> items, QUrl url);

private:
    FMH::FileLoader *m_loader;
    QFileSystemWatcher *m_watcher;

    FMH::MODEL_LIST m_list;
    QString m_nameFilters;
    QUrl m_url;
    bool m_dirOnly = false;
    bool m_showDotFiles = false;

    bool m_checking = false;

    void reviewChanges();
    bool includes(const QUrl &url);
    int indexOf(const FMH::MODEL_KEY &key, const QString &value) const;
};
#endif

class Syncing;
class Tagging;

/**
 * @brief The FM class
 * File management methods with syncing and tagging integration if such components were enabled with COMPONENT_SYNCING and COMPONENT_TAGGING
 */
class FM : public QObject
{
    Q_OBJECT

public:
    FM(QObject *parent = nullptr);

    /** Syncing **/
    /**
     * @brief getCloudServerContent
     * Given a server URL address return the contents. This only works if the syncing component has been enabled COMPONENT_SYNCING
     * @param server
     * Server URL
     * @param filters
     * Filters to be applied
     * @param depth
     * How deep in the directory three go, for example 1 keeps the retrieval in the first level
     * @return
     */
    bool getCloudServerContent(const QUrl &server, const QStringList &filters = QStringList(), const int &depth = 0);

    /**
     * @brief createCloudDir
     * Creates a directory in the server. This only works if the syncing component has been enabled COMPONENT_SYNCING
     * @param path
     * Server address URL
     * @param name
     * Directory name
     */
    Q_INVOKABLE void createCloudDir(const QString &path, const QString &name);

    /**
     * @brief getPathContent
     * Given a path URL extract the contents and return the information packaged as a model. This method is asyncronous and once items are ready signals are emitted, such as: pathContentItemsReady or pathContentReady
     * @param path
     * The directory path
     * @param hidden
     * If shoudl also pack hidden files
     * @param onlyDirs
     * Should only pack directories
     * @param filters
     * Filters to be applied to the retrieval
     * @param iteratorFlags
     * Directory iterator flags, for reference check QDirIterator documentation
     */
    void getPathContent(const QUrl &path, const bool &hidden = false, const bool &onlyDirs = false, const QStringList &filters = QStringList(), const QDirIterator::IteratorFlags &iteratorFlags = QDirIterator::NoIteratorFlags);

    /**
     * @brief resolveLocalCloudPath
     * Given a server address URL resolve it to the local cache URL. This only works if the syncing component has been enabled COMPONENT_SYNCING
     * @param path
     * Server address
     * @return
     */
    QString resolveLocalCloudPath(const QString &path);

    /**
     * @brief getAppsPath
     * Gives the path to the applications directory. Missing integration with other system other than GNU Linux
     * @return
     */
    static FMH::MODEL_LIST getAppsPath();

    /**
     * @brief resolveUserCloudCachePath
     * @param server
     * @param user
     * @return
     */
    static QString resolveUserCloudCachePath(const QString &server, const QString &user);

#ifdef COMPONENT_SYNCING
    Syncing *sync;
#endif

private:
#ifdef COMPONENT_TAGGING
    Tagging *tag;
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    KCoreDirLister *dirLister;
#else
    QDirLister *dirLister;
#endif

signals:
    void cloudServerContentReady(FMH::MODEL_LIST list, const QUrl &url);
    void cloudItemReady(FMH::MODEL item, QUrl path); // when a item is downloaded and ready

    void pathContentReady(QUrl path);
    void pathContentItemsReady(FMH::PATH_CONTENT list);
    void pathContentChanged(QUrl path);
    void pathContentItemsChanged(QVector<QPair<FMH::MODEL, FMH::MODEL>> items);
    void pathContentItemsRemoved(FMH::PATH_CONTENT list);

    void warningMessage(QString message);
    void loadProgress(int percent);

    void dirCreated(FMH::MODEL dir);
    void newItem(FMH::MODEL item, QUrl path); // when a new item is created

public slots:
    void openCloudItem(const QVariantMap &item);
    void getCloudItem(const QVariantMap &item);

    /* ACTIONS */
    bool copy(const QList<QUrl> &urls, const QUrl &where);
    bool cut(const QList<QUrl> &urls, const QUrl &where);

    friend class FMStatic;
};

#endif // FM_H
