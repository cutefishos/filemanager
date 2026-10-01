// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATHBARMODEL_H
#define PATHBARMODEL_H

#include <QAbstractItemModel>
#include <QUrl>

struct PathBarItem {
    QString name;
    QUrl url;
};

class PathBarModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)

public:
    enum DataRole {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        PathRole
    };
    Q_ENUMS(DataRole);

    explicit PathBarModel(QObject *parent = nullptr);
    ~PathBarModel();

    QString url() const;
    void setUrl(const QString &url);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

signals:
    void urlChanged();

private:
    QString m_url;
    QList<PathBarItem *> m_pathList;
};

#endif // PATHBARMODEL_H
