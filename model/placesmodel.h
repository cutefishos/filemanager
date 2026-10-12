// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLACESMODEL_H
#define PLACESMODEL_H

#include "placesitem.h"
#include <QAbstractItemModel>
#include <volumemanager.h>

class PlacesModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(int favoriteCount READ favoriteCount NOTIFY favoritesChanged)
    Q_PROPERTY(QStringList recentServers READ recentServers NOTIFY recentServersChanged)

public:
    enum DataRole {
        NameRole = Qt::UserRole + 1,
        IconNameRole,
        UrlRole,
        PathRole,
        IsDeviceRole,
        IsOpticalDisc,
        setupNeededRole,
        CanEjectRole,
        CategoryRole
    };

    Q_ENUMS(DataRole);

    explicit PlacesModel(QObject *parent = nullptr);
    ~PlacesModel() override;

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    Q_INVOKABLE QVariantMap get(const int &index) const;
    int favoriteCount() const;
    Q_INVOKABLE bool canAddFavorites(const QList<QUrl> &urls) const;
    Q_INVOKABLE bool addFavorites(const QList<QUrl> &urls, int before);
    Q_INVOKABLE bool moveFavorite(int from, int before);
    Q_INVOKABLE bool canDropInto(const QList<QUrl> &urls, int row, int action) const;
    Q_INVOKABLE bool dropInto(const QList<QUrl> &urls, int row, int action);
    Q_INVOKABLE void removeFavorite(int row);
    Q_INVOKABLE void requestSetup(const int &index);
    Q_INVOKABLE void requestEject(const int &index);
    Q_INVOKABLE void requestTeardown(const int &index);

    QStringList recentServers() const;
    Q_INVOKABLE void removeRecentServer(const QString &address);
    // Connects to a server, asking through passwordRequested and hostKeyRequested.
    Q_INVOKABLE void connectToServer(const QString &address);
    Q_INVOKABLE void answerPassword(const QString &address, const QString &user,
                                    const QString &domain, const QString &password,
                                    bool anonymous, bool remember);
    Q_INVOKABLE void acceptHostKey(const QString &address, const QString &fingerprint);
    Q_INVOKABLE void cancelConnection(const QString &address);

signals:
    void favoritesChanged();
    void deviceSetupDone(const QString &filePath);
    void deviceOperationFailed(const QString &message);
    void recentServersChanged();
    void passwordRequested(const QString &address, const QString &user, const QString &domain,
                           bool needsDomain, bool anonymousAllowed, bool retry);
    void hostKeyRequested(const QString &address, const QString &fingerprint, bool changed);

private slots:
    void onDeviceAdded(const QString &udi);
    void onDeviceRemoved(const QString &udi);
    void onItemChanged(PlacesItem *);

private:
    void saveFavorites();
    void syncMounts();
    void insertLocation(PlacesItem *item, const QString &category);
    void mountLocation(const QString &address);
    void addRecentServer(const QString &address);
    QList<PlacesItem *> m_items;
    Cutefish::IO::VolumeManager *m_volumeManager;
    // Answers given so far while connecting to each address.
    QHash<QString, QVariantMap> m_connecting;
};

#endif
