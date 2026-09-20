#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

class FolderSizeCache : public QObject
{
    Q_OBJECT

public:
    explicit FolderSizeCache(QObject *parent = nullptr);
    ~FolderSizeCache() override;

    qint64 count(const QString &path);
    void clear();

signals:
    void ready(const QString &path);

private:
    QHash<QString, qint64> m_counts;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};
