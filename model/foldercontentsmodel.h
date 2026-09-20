#pragma once

#include <directorymodel.h>
#include <QAbstractListModel>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <atomic>
#include <memory>

struct FolderQuery
{
    int sortColumn = Cutefish::IO::DirectoryModel::Name;
    bool descending = false;
    bool directoriesFirst = true;
    int filterMode = 0;
    bool matchAllNames = true;
    QSet<QString> mimeTypes;
    QList<QRegularExpression> patterns;
};

class FolderContentsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit FolderContentsModel(QObject *parent = nullptr);
    ~FolderContentsModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;

    QModelIndex parent(const QModelIndex &) const override
    {
        return {};
    }

    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

    void setSourceModel(Cutefish::IO::DirectoryModel *source);
    Cutefish::IO::DirectoryModel *sourceModel() const
    {
        return m_source;
    }

    QModelIndex mapToSource(const QModelIndex &index) const;
    QModelIndex mapFromSource(const QModelIndex &index) const;
    Cutefish::IO::FileItem snapshotItem(const QModelIndex &index) const;
    QModelIndex viewIndexForUrl(const QUrl &url) const;

    void setQuery(const FolderQuery &query);
    void schedulePreparation();
    void cancelPreparation();

signals:
    void preparationStarted();
    void preparationFinished();

private:
    struct Prepared;
    void prepare();
    void publishBatch();
    void clear();

    Cutefish::IO::DirectoryModel *m_source = nullptr;
    Cutefish::IO::FileItemList m_items;
    QHash<QUrl, int> m_rowsByUrl;
    bool m_rowsValid = true;
    FolderQuery m_query;
    QTimer m_prepareTimer;
    QTimer m_publishTimer;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    std::shared_ptr<Prepared> m_prepared;
    int m_updateOffset = 0;
    int m_insertOffset = 0;
};
