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

#include "desktopview.h"

#include "model/foldermodel.h"
#include "qmltypes.h"

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>

static const char *kDesktopRootQml = "qrc:/qml/Desktop/Main.qml";

DesktopView::DesktopView(QQuickItem *parent)
    : QQuickItem(parent)
    , m_component(nullptr)
    , m_context(nullptr)
    , m_content(nullptr)
    , m_menuBar(new QMenu)
    , m_fileMenu(m_menuBar->addMenu(tr("File")))
    , m_editMenu(m_menuBar->addMenu(tr("Edit")))
    , m_newWindowAction(new QAction(tr("New Window"), this))
    , m_selectAllAction(new QAction(tr("Select All"), this))
{
    setFlag(ItemHasContents, false);
    CutefishFM::initResources();

    // Same refresh the context menu asks for before it opens.
    for (QMenu *menu : {m_fileMenu, m_editMenu}) {
        connect(menu, &QMenu::aboutToShow, this, [this] {
            if (m_folderModel)
                m_folderModel->prepareContextMenu();
        });
    }
}

DesktopView::~DesktopView()
{
    delete m_menuBar;
}

QScreen *DesktopView::screen() const
{
    return m_screen.data();
}

void DesktopView::setScreen(QScreen *screen)
{
    if (m_screen == screen)
        return;

    if (m_screen)
        disconnect(m_screen, nullptr, this, nullptr);

    m_screen = screen;

    if (m_screen) {
        connect(m_screen, &QScreen::geometryChanged, this, &DesktopView::updateScreenRect);
        connect(m_screen, &QScreen::virtualGeometryChanged, this, &DesktopView::updateScreenRect);
    }

    emit screenChanged();
    updateScreenRect();
}

QString DesktopView::screenName() const
{
    return m_screen ? m_screen->name() : QString();
}

QRect DesktopView::screenRect() const
{
    return m_screenRect;
}

QObject *DesktopView::folderModel() const
{
    return m_folderModel.data();
}

void DesktopView::setFolderModel(QObject *model)
{
    FolderModel *folderModel = qobject_cast<FolderModel *>(model);

    if (m_folderModel == folderModel)
        return;

    if (m_folderModel) {
        disconnect(m_newWindowAction, nullptr, m_folderModel, nullptr);
        disconnect(m_selectAllAction, nullptr, m_folderModel, nullptr);
    }

    m_folderModel = folderModel;

    if (m_folderModel) {
        // A window opened from the desktop starts at home, not at ~/Desktop.
        connect(m_newWindowAction, &QAction::triggered, m_folderModel, [this] {
            m_folderModel->openInNewWindow(m_folderModel->homePath());
        });
        connect(m_selectAllAction, &QAction::triggered, m_folderModel, &FolderModel::selectAll);
    }

    rebuildMenuBar();
    emit folderModelChanged();
}

QMenu *DesktopView::menuBar() const
{
    return m_menuBar;
}

// The model's own actions are shared with the context menu, so their enabled
// state (writable folder, clipboard contents, selection) is already kept up
// to date there.
void DesktopView::rebuildMenuBar()
{
    m_fileMenu->clear();
    m_editMenu->clear();

    if (!m_folderModel)
        return;

    m_fileMenu->addAction(m_newWindowAction);
    m_fileMenu->addAction(m_folderModel->action(QStringLiteral("newFolder")));
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_folderModel->action(QStringLiteral("properties")));

    m_editMenu->addAction(m_folderModel->action(QStringLiteral("undo")));
    m_editMenu->addAction(m_folderModel->action(QStringLiteral("redo")));
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_folderModel->action(QStringLiteral("cut")));
    m_editMenu->addAction(m_folderModel->action(QStringLiteral("copy")));
    m_editMenu->addAction(m_folderModel->action(QStringLiteral("paste")));
    m_editMenu->addAction(m_selectAllAction);
}

void DesktopView::updateScreenRect()
{
    const QRect rect = m_screen ? m_screen->geometry() : QRect();

    if (m_screenRect == rect)
        return;

    m_screenRect = rect;
    emit screenRectChanged();
}

void DesktopView::componentComplete()
{
    QQuickItem::componentComplete();
    createContent();
}

void DesktopView::createContent()
{
    if (m_content)
        return;

    QQmlEngine *engine = qmlEngine(this);
    if (!engine) {
        qWarning() << "DesktopView: no QML engine, cannot create the desktop.";
        return;
    }

    CutefishFM::registerQmlTypes();
    CutefishFM::registerImageProviders(engine);

    // A dedicated child context keeps the desktop's context properties out of
    // the host's root context, so several desktops (one per screen) can live in
    // one engine without seeing each other's state.
    m_context = new QQmlContext(qmlContext(this), this);
    m_context->setContextProperty(QStringLiteral("desktopView"), this);

    m_component = new QQmlComponent(engine, QUrl(QString::fromLatin1(kDesktopRootQml)), this);

    if (m_component->isError()) {
        qWarning() << "DesktopView: failed to load" << kDesktopRootQml << m_component->errors();
        return;
    }

    QObject *object = m_component->beginCreate(m_context);
    m_content = qobject_cast<QQuickItem *>(object);

    if (!m_content) {
        qWarning() << "DesktopView: the desktop root is not an Item." << m_component->errors();
        delete object;
        return;
    }

    m_content->setParentItem(this);
    resizeContent();
    m_component->completeCreate();

    if (m_component->isError())
        qWarning() << "DesktopView:" << m_component->errors();
}

void DesktopView::resizeContent()
{
    if (m_content) {
        m_content->setPosition(QPointF(0, 0));
        m_content->setSize(size());
    }
}

void DesktopView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    resizeContent();
}
