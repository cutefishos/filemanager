/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CUTEFISH_DESKTOPVIEW_H
#define CUTEFISH_DESKTOPVIEW_H

#include <QMenu>
#include <QPointer>
#include <QQuickItem>
#include <QRect>

class FolderModel;
class QAction;
class QQmlComponent;
class QQmlContext;
class QScreen;

/**
 * The whole Cutefish desktop as a single QML item.
 *
 * Everything the desktop does -- icons, layout, selection, drag and drop,
 * context menus and file operations -- lives behind this item and reuses the
 * file manager core. The background is not part of it: the shell draws that
 * under the item. A host only has to place the item in a window and tell it
 * which screen it is on:
 *
 *     import org.cutefish.filemanager.desktop 1.0
 *
 *     DesktopView {
 *         anchors.fill: parent
 *         screen: Qt.application.screens[0]
 *     }
 *
 * The item owns no window state (flags, type hints, stacking); that is the
 * host's job, so the same item works on X11, Wayland or inside a test harness.
 *
 * menuBar is what a global menu bar shows while the desktop has the focus: a
 * QMenu whose top-level actions are the "File" and "Edit" menus, acting on the
 * same folder model as the icons.
 */
class DesktopView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QScreen *screen READ screen WRITE setScreen NOTIFY screenChanged)
    Q_PROPERTY(QString screenName READ screenName NOTIFY screenChanged)
    Q_PROPERTY(QRect screenRect READ screenRect NOTIFY screenRectChanged)

    Q_PROPERTY(qreal leftMargin MEMBER m_leftMargin NOTIFY marginsChanged)
    Q_PROPERTY(qreal topMargin MEMBER m_topMargin NOTIFY marginsChanged)
    Q_PROPERTY(qreal rightMargin MEMBER m_rightMargin NOTIFY marginsChanged)
    Q_PROPERTY(qreal bottomMargin MEMBER m_bottomMargin NOTIFY marginsChanged)

    // Set by the desktop's own QML once its folder model exists.
    Q_PROPERTY(QObject *folderModel READ folderModel WRITE setFolderModel NOTIFY folderModelChanged)
    Q_PROPERTY(QMenu *menuBar READ menuBar CONSTANT)

public:
    explicit DesktopView(QQuickItem *parent = nullptr);
    ~DesktopView() override;

    QScreen *screen() const;
    void setScreen(QScreen *screen);

    QString screenName() const;
    QRect screenRect() const;

    QObject *folderModel() const;
    void setFolderModel(QObject *model);

    QMenu *menuBar() const;

signals:
    void marginsChanged();
    void screenChanged();
    void screenRectChanged();
    void folderModelChanged();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void createContent();
    void updateScreenRect();
    void resizeContent();
    void rebuildMenuBar();

    QQmlComponent *m_component;
    QQmlContext *m_context;
    QQuickItem *m_content;
    QPointer<QScreen> m_screen;
    QRect m_screenRect;
    QPointer<FolderModel> m_folderModel;
    QMenu *m_menuBar;
    QMenu *m_fileMenu;
    QMenu *m_editMenu;
    QAction *m_newWindowAction;
    QAction *m_selectAllAction;
    qreal m_leftMargin = 0;
    qreal m_topMargin = 0;
    qreal m_rightMargin = 0;
    qreal m_bottomMargin = 0;
};

#endif // CUTEFISH_DESKTOPVIEW_H
