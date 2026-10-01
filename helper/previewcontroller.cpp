#include "previewcontroller.h"
#include "helper/filelauncher.h"
#include "mimetype/mimeappmanager.h"
#include "model/positioner.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QKeyEvent>
#include <QMimeDatabase>
#include <QtConcurrentRun>
#include <filesystem.h>

namespace {
QVariantMap describeFile(const QUrl &url)
{
    QVariantMap result{{"url", url}, {"name", url.fileName()}, {"kind", "generic"}};
    if (!url.isLocalFile()) {
        result.insert("message", PreviewController::tr("Preview is unavailable for this location."));
        return result;
    }

    const QFileInfo info(url.toLocalFile());
    result.insert("name", info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName());
    if (!info.exists() || (!info.isFile() && !info.isDir())) {
        result.insert("message", PreviewController::tr("This file is no longer available."));
        return result;
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    result.insert("type", mime.comment());
    result.insert("icon", mime.iconName());
    result.insert("size", Cutefish::IO::convertSize(Cutefish::IO::FileItem(url).size()));
    if (info.isDir()) {
        result.insert("folder", true);
        result.insert("icon", "folder");
        quint64 count = 0;
        QDirIterator entries(info.filePath(), QDir::AllEntries | QDir::NoDotAndDotDot
                             | QDir::Hidden | QDir::System);
        while (entries.hasNext()) {
            entries.next();
            ++count;
        }
        result.insert("itemCount", count);
        return result;
    }

    const QString name = mime.name();
    if (name.startsWith("image/")) {
        QImageReader reader(info.filePath());
        if (reader.canRead()) {
            result.insert("kind", "image");
            QSize size = reader.size();
            if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                size.transpose();
            if (size.isValid()) {
                result.insert("width", size.width());
                result.insert("height", size.height());
            }
            result.insert("vector", mime.inherits("image/svg+xml")
                                    || mime.inherits("image/svg+xml-compressed"));
            result.insert("animated", reader.supportsAnimation() && reader.imageCount() != 1);
        }
    }

    return result;
}
}

PreviewController::PreviewController(QObject *parent)
    : QObject(parent)
{
    // Selection changes can clear and replace the selection in the same event.
    m_selectionTimer.setSingleShot(true);
    connect(&m_selectionTimer, &QTimer::timeout, this, &PreviewController::synchronize);
}

PreviewController::~PreviewController()
{
    stopFolderSize();
}

bool PreviewController::visible() const
{
    return m_visible;
}

QVariantList PreviewController::items() const
{
    return m_items;
}

int PreviewController::currentIndex() const
{
    return m_index;
}

QVariantMap PreviewController::file() const
{
    return m_file;
}

bool PreviewController::loading() const
{
    return m_loading;
}

void PreviewController::attach(FolderModel *model, QQuickItem *view, QQuickWindow *window)
{
    close();
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    if (m_view) {
        m_view->removeEventFilter(this);
        disconnect(m_view, nullptr, this, nullptr);
    }
    if (m_window)
        m_window->removeEventFilter(this);
    m_model = model;
    m_view = view;
    m_window = window;
    if (m_view)
        m_view->installEventFilter(this);
    if (m_window)
        m_window->installEventFilter(this);
    if (!m_model)
        return;
    connect(m_model, &FolderModel::selectionCountChanged, this, [this] {
        if (m_visible)
            m_selectionTimer.start();
    });
    connect(m_model, &FolderModel::urlChanged, this, [this] {
        if (m_visible)
            m_selectionTimer.start();
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this] {
        if (m_visible)
            m_selectionTimer.start();
    });
}

void PreviewController::toggle()
{
    if (m_visible) {
        close();
        return;
    }
    if (!m_model || !m_view
        || m_view->property("editor").value<QObject *>())
        return;
    m_visible = true;
    synchronize();
    if (m_visible)
        emit showRequested();
}

void PreviewController::close()
{
    if (!m_visible)
        return;
    m_visible = false;
    m_showPending = false;
    ++m_generation;
    stopFolderSize();
    m_selectionTimer.stop();
    // The shared window keeps its last frame while hidden and during reopening.
    m_loading = false;
    emit changed();
    emit fileChanged();
    emit closed();
    if (m_view && m_view->isVisible()) {
        m_view->forceActiveFocus();
        if (m_view->window() && m_view->window()->isVisible())
            m_view->window()->requestActivate();
    }
}

QVariantMap PreviewController::snapshot(FolderModel *model, QQuickItem *view)
{
    QVariantList items;
    int currentIndex = 0;
    int currentRow = view ? view->property("currentIndex").toInt() : -1;
    if (view) {
        if (auto *positioner = view->property("positioner").value<Positioner *>())
            currentRow = positioner->map(currentRow);
    }
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!model->isSelected(row))
            continue;
        if (row == currentRow)
            currentIndex = items.size();
        const QModelIndex index = model->index(row, 0);
        items.append(QVariantMap{
            {"url", model->data(index, FolderModel::UrlRole)},
            {"name", model->data(index, FolderModel::DisplayNameRole)},
            {"icon", model->data(index, FolderModel::IconNameRole)},
            {"thumbnail", model->data(index, FolderModel::ThumbnailRole)}
        });
    }
    if (items.isEmpty()) {
        const QUrl url = model->resolvedUrl();
        items.append(QVariantMap{{"url", url}, {"name", url.fileName()}, {"icon", "folder"}});
    }
    const qreal ratio = view && view->window() ? view->window()->devicePixelRatio() : 0;
    return {{"items", items}, {"currentIndex", currentIndex}, {"ratio", ratio}};
}

void PreviewController::setWindow(QQuickWindow *window)
{
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (m_window)
        m_window->installEventFilter(this);
}

void PreviewController::present(const QVariantList &items, int currentIndex, bool activate,
                                qreal ratio)
{
    if (items.isEmpty())
        return;
    if (ratio > 0)
        m_sourceRatio = ratio;
    const bool wasVisible = m_visible;
    m_visible = true;
    if (items != m_items || !wasVisible) {
        const QUrl previous = m_index < m_items.size()
                ? m_items.at(m_index).toMap().value("url").toUrl() : QUrl();
        m_items = items;
        m_index = qBound(0, currentIndex, int(items.size()) - 1);
        if (wasVisible && !activate) {
            for (int i = 0; i < items.size(); ++i) {
                if (items.at(i).toMap().value("url").toUrl() == previous)
                    m_index = i;
            }
        }
        emit changed();
        load();
    }
    if (activate) {
        if (m_loading) {
            // Open at the size of what is shown, unless describing it stalls.
            m_showPending = true;
            const quint64 generation = m_generation;
            QTimer::singleShot(250, this, [this, generation] {
                if (m_showPending && generation == m_generation) {
                    m_showPending = false;
                    emit showRequested();
                }
            });
        } else {
            emit showRequested();
        }
    }
}

void PreviewController::synchronize()
{
    if (!m_visible || !m_model)
        return;
    const auto state = snapshot(m_model, m_view);
    present(state.value("items").toList(), state.value("currentIndex").toInt(), false,
            state.value("ratio").toReal());
}

void PreviewController::select(int index)
{
    if (!m_visible || index < 0 || index >= m_items.size() || index == m_index)
        return;
    m_index = index;
    emit changed();
    load();
}

void PreviewController::step(int delta)
{
    select(m_index + delta);
}

void PreviewController::openCurrent()
{
    const QString desktopFile = m_file.value("application").toMap().value("desktopFile").toString();
    const QUrl url = m_file.value("url").toUrl();
    if (!m_visible || desktopFile.isEmpty() || !url.isLocalFile())
        return;
    if (FileLauncher::self()->launchApp(desktopFile, url.toLocalFile()))
        close();
}

void PreviewController::load()
{
    stopFolderSize();
    const QUrl url = m_items.at(m_index).toMap().value("url").toUrl();
    const quint64 generation = ++m_generation;
    if (m_file.isEmpty())
        m_file = m_items.at(m_index).toMap();
    m_loading = true;
    emit fileChanged();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        if (generation == m_generation && m_visible) {
            m_file = watcher->result();
            if (!m_file.value("folder").toBool() && m_file.value("url").toUrl().isLocalFile()) {
                const QVariantList apps = MimeAppManager::self()->recommendedApps(m_file.value("url").toUrl());
                if (!apps.isEmpty())
                    m_file.insert("application", apps.first());
            }
            m_loading = false;
            emit fileChanged();
            if (m_showPending) {
                m_showPending = false;
                emit showRequested();
            }
            if (m_file.value("folder").toBool())
                loadFolderSize(m_file.value("url").toUrl());
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(describeFile, url));
}

void PreviewController::stopFolderSize()
{
    if (!m_sizeJob)
        return;
    disconnect(m_sizeJob, nullptr, this, nullptr);
    m_sizeJob->stop();
    m_sizeJob->deleteLater();
    m_sizeJob = nullptr;
}

void PreviewController::loadFolderSize(const QUrl &url)
{
    m_sizeJob = new CFileSizeJob(this);
    const auto updateSize = [this] {
        m_file.insert("size", Cutefish::IO::convertSize(m_sizeJob->totalSize()));
        m_file.insert("sizeBytes", m_sizeJob->totalSize());
        emit fileChanged();
    };
    connect(m_sizeJob, &CFileSizeJob::sizeChanged, this, updateSize);
    connect(m_sizeJob, &CFileSizeJob::result, this, updateSize);
    m_sizeJob->start({url});
}

bool PreviewController::eventFilter(QObject *object, QEvent *event)
{
    if (!m_visible || (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride))
        return QObject::eventFilter(object, event);
    auto *key = static_cast<QKeyEvent *>(event);
    const auto modifiers = key->modifiers();
    if (event->type() == QEvent::ShortcutOverride) {
        // Tool windows also match their parent's window-scoped shortcuts.
        if (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Space
            || (modifiers == Qt::ControlModifier && key->key() == Qt::Key_W)) {
            key->accept();
            return true;
        }
        return false;
    }
    if ((key->key() == Qt::Key_Space && modifiers == Qt::NoModifier)
        || (key->key() == Qt::Key_W && modifiers == Qt::ControlModifier)) {
        if (!key->isAutoRepeat())
            close();
        return true;
    }
    if (key->key() == Qt::Key_Escape) {
        close();
        return true;
    }
    if (m_items.size() > 1 && modifiers == Qt::NoModifier
        && (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right)) {
        step(key->key() == Qt::Key_Left ? -1 : 1);
        return true;
    }
    if (object == m_window && key->key() >= Qt::Key_Left && key->key() <= Qt::Key_Down) {
        if (m_view) {
            QKeyEvent forwarded(key->type(), key->key(), modifiers, key->text(), key->isAutoRepeat());
            QCoreApplication::sendEvent(m_view, &forwarded);
        } else {
            emit navigateRequested(key->key(), int(modifiers));
        }
        return true;
    }
    return QObject::eventFilter(object, event);
}
