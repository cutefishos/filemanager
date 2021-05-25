/***************************************************************************
 *   Copyright (C) 2006 David Faure <faure@kde.org>                        *
 *   Copyright (C) 2008 Fredrik Höglund <fredrik@kde.org>                  *
 *   Copyright (C) 2008 Rafael Fernández López <ereslibre@kde.org>         *
 *   Copyright (C) 2011 Marco Martin <mart@kde.org>                        *
 *   Copyright (C) 2014 by Eike Hein <hein@kde.org>                        *
 *   Copyright (C) 2021 Reven Martin <revenmartin@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef FOLDERMODEL_H
#define FOLDERMODEL_H

#include "../widgets/itemviewadapter.h"
#include "../helper/pathhistory.h"

#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QQmlParserStatus>
#include <QQuickItem>

#include <KDirLister>
#include <KDirModel>
#include <KDirWatch>
#include <KActionCollection>

class QDrag;
class FolderModel : public QSortFilterProxyModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QUrl resolvedUrl READ resolvedUrl NOTIFY resolvedUrlChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool sortDirsFirst READ sortDirsFirst WRITE setSortDirsFirst NOTIFY sortDirsFirstChanged)
    Q_PROPERTY(bool dragging READ dragging NOTIFY draggingChanged)
    Q_PROPERTY(QObject *viewAdapter READ viewAdapter WRITE setViewAdapter NOTIFY viewAdapterChanged)
    Q_PROPERTY(bool isDesktop READ isDesktop WRITE setIsDesktop NOTIFY isDesktopChanged)
    Q_PROPERTY(int selectionCound READ selectionCound NOTIFY selectionCoundChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum DataRole {
        BlankRole = Qt::UserRole + 1,
        SelectedRole,
        IsDirRole,
        UrlRole,
        FileNameRole,
        FileSizeRole,
        IconNameRole,
        ThumbnailRole,
        ModifiedRole
    };

    enum FilterMode {
        NoFilter = 0,
        FilterShowMatches,
        FilterHideMatches,
    };

    enum Status {
        None,
        Ready,
        Listing,
        Canceled,
    };
    Q_ENUM(Status)

    struct DragImage {
        int row;
        QRect rect;
        QPoint cursorOffset;
        QImage image;
        bool blank;
    };

    explicit FolderModel(QObject *parent = nullptr);
    ~FolderModel() override;

    void classBegin() override;
    void componentComplete() override;

    QHash<int, QByteArray> roleNames() const override;
    static QHash<int, QByteArray> staticRoleNames();
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    KFileItem itemForIndex(const QModelIndex &index) const;

    QList<QUrl> selectedUrls() const;

    QString url() const;
    void setUrl(const QString &url);

    QUrl resolvedUrl() const;
    Q_INVOKABLE QUrl resolve(const QString &url);

    Status status() const;
    void setStatus(Status status);

    int sortMode() const;
    void setSortMode(int mode);

    bool sortDirsFirst() const;
    void setSortDirsFirst(bool enable);

    QObject *viewAdapter() const;
    void setViewAdapter(QObject *adapter);

    bool dragging() const;

    bool isDir(const QModelIndex &index, const KDirModel *dirModel) const;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;

    KFileItem rootItem() const;

    int count() const;
    int selectionCound() const;

    Q_INVOKABLE QString homePath() const;
    Q_INVOKABLE QString desktopPath() const;
    Q_INVOKABLE QAction *action(const QString &name) const;

    Q_INVOKABLE void up();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();

    Q_INVOKABLE bool supportSetAsWallpaper(const QString &mimeType);
    Q_INVOKABLE int fileExtensionBoundary(int row);

    Q_INVOKABLE bool hasSelection() const;
    Q_INVOKABLE bool isSelected(int row) const;
    Q_INVOKABLE bool isBlank(int row) const;
    Q_INVOKABLE void setSelected(int row);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void setRangeSelected(int anchor, int to);
    Q_INVOKABLE void updateSelection(const QVariantList &rows, bool toggle);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void pinSelection();
    Q_INVOKABLE void unpinSelection();

    Q_INVOKABLE void newFolder();
    Q_INVOKABLE void rename(int row, const QString &name);
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void openSelected();
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void moveSelectedToTrash();
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE void keyDeletePress();

    Q_INVOKABLE void setDragHotSpotScrollOffset(int x, int y);
    Q_INVOKABLE void addItemDragImage(int row, int x, int y, int width, int height, const QVariant &image);
    Q_INVOKABLE void clearDragImages();
    Q_INVOKABLE void dragSelected(int x, int y);

    Q_INVOKABLE void setWallpaperSelected();

    Q_INVOKABLE void openContextMenu(QQuickItem *visualParent = nullptr, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    Q_INVOKABLE void openPropertiesDialog();
    Q_INVOKABLE void openInTerminal();
    Q_INVOKABLE void openChangeWallpaperDialog();

    bool isDesktop() const;
    void setIsDesktop(bool isDesktop);

signals:
    void urlChanged();
    void resolvedUrlChanged();
    void statusChanged();
    void sortModeChanged();
    void sortDescChanged();
    void sortDirsFirstChanged();
    void requestRename();
    void draggingChanged();
    void viewAdapterChanged();
    void isDesktopChanged();
    void selectionCoundChanged();
    void countChanged();

private slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void dragSelectedInternal(int x, int y);

private:
    void invalidateIfComplete();
    void invalidateFilterIfComplete();
    void createActions();
    void updateActions();
    void addDragImage(QDrag *drag, int x, int y);

    bool isSupportThumbnails(const QString &mimeType) const;

private:
    KDirModel *m_dirModel;
    KDirWatch *m_dirWatch;

    QItemSelectionModel *m_selectionModel;
    QItemSelection m_pinnedSelection;
    QString m_url;

    Status m_status;
    int m_sortMode;
    bool m_sortDesc;
    bool m_sortDirsFirst;

    bool m_complete;
    bool m_isDesktop;

    KActionCollection m_actionCollection;
    QHash<int, DragImage *> m_dragImages;
    QModelIndexList m_dragIndexes;
    QPoint m_dragHotSpotScrollOffset;
    bool m_dragInProgress;

    QPointer<ItemViewAdapter> m_viewAdapter;

    // Save path history
    PathHistory m_pathHistory;
};

#endif // FOLDERMODEL_H
