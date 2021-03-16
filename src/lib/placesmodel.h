#ifndef PLACESMODEL_H
#define PLACESMODEL_H

#include <QAbstractItemModel>
#include "placesitem.h"

class PlacesModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum DataRole {
        NameRole = Qt::UserRole + 1,
        IconNameRole,
        IconPathRole,
        UrlRole,
        PathRole
    };
    Q_ENUMS(DataRole);

    explicit PlacesModel(QObject *parent = nullptr);
    ~PlacesModel() override;

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    Q_INVOKABLE QVariantMap get(const int &index) const;

private:
    QList<PlacesItem *> m_items;
};

#endif
