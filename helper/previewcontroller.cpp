#include "previewcontroller.h"
#include "helper/filelauncher.h"
#include "mimetype/mimeappmanager.h"
#include "model/positioner.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImageReader>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeDatabase>
#include <QStringDecoder>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtConcurrentRun>
#include <filesystem.h>
#include <memory>
#include <optional>
#include <unicode/ucsdet.h>

// The text item lays out everything it holds when it is shown.
static constexpr qint64 textLimit = 256 * 1024;
// TextEdit builds a paragraph's nodes whole, and at either end of the text it
// rebuilds the paragraphs in view on every scroll step.
static constexpr qsizetype paragraphLimit = 4000;

// Where PreviewTextView's TextEdit starts each wrapped line of a paragraph.
static QList<int> lineStarts(QStringView paragraph, const QFont &font, qreal width)
{
    // TextEdit rounds the font to half points, and with the theme's
    // Text.QtRendering it lays out with design metrics.
    QFont rounded = font;
    if (rounded.pointSizeF() > 0)
        rounded.setPointSizeF(qRound(rounded.pointSizeF() * 2) / 2.0);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setUseDesignMetrics(true);
    option.setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * 8);

    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(rounded);
    document.setDefaultTextOption(option);
    document.setTextWidth(width);
    document.setPlainText(paragraph.toString());
    // Lays out the whole document, which otherwise happens lazily.
    document.size();

    QList<int> starts;
    const QTextLayout *layout = document.firstBlock().layout();
    for (int i = 1; i < layout->lineCount(); ++i)
        starts.append(layout->lineAt(i).textStart());
    return starts;
}

// Breaks longer paragraphs where TextEdit wraps a line at `width`, so a
// paragraph reads as one, or without a width after a space near the limit
// where there is one. Returns the positions of the line breaks it inserted.
static QVariantList splitLongParagraphs(QString *text, const QFont &font, qreal width)
{
    QVariantList breaks;
    QString result;
    result.reserve(text->size() + text->size() / paragraphLimit);
    qsizetype start = 0;
    while (start < text->size()) {
        qsizetype end = text->indexOf(QLatin1Char('\n'), start);
        end = end < 0 ? text->size() : end + 1;
        QList<int> starts;
        if (end - start > paragraphLimit && width > 0)
            starts = lineStarts(QStringView(*text).sliced(start, end - start), font, width);
        const qsizetype paragraph = start;
        auto line = starts.cbegin();
        while (end - start > paragraphLimit) {
            qsizetype cut = start + paragraphLimit;
            while (line != starts.cend() && paragraph + *line <= cut)
                ++line;
            const qsizetype wrap = line == starts.cbegin() ? -1 : paragraph + *(line - 1);
            const qsizetype space = text->lastIndexOf(QLatin1Char(' '), cut - 1);
            if (wrap > start)
                cut = wrap;
            else if (space >= cut - 200 && space > start)
                cut = space + 1;
            else if (text->at(cut).isLowSurrogate())
                --cut;
            result.append(QStringView(*text).sliced(start, cut - start));
            breaks.append(result.size());
            result.append(QLatin1Char('\n'));
            start = cut;
        }
        result.append(QStringView(*text).sliced(start, end - start));
        start = end;
    }
    if (!breaks.isEmpty())
        *text = result;
    return breaks;
}

// A decoder for text that is neither UTF-8 nor marked by a byte order mark
// (GBK, Shift JIS, Big5, Windows code pages), guessed from byte statistics.
static QStringDecoder legacyDecoder(const QByteArray &data)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UCharsetDetector, decltype(&ucsdet_close)> detector(ucsdet_open(&status),
                                                                        ucsdet_close);
    ucsdet_setText(detector.get(), data.constData(), int32_t(data.size()), &status);
    int32_t count = 0;
    const UCharsetMatch **matches = ucsdet_detectAll(detector.get(), &count, &status);
    if (U_FAILURE(status))
        return {};

    // A few characters score the same in several encodings; the UI language
    // settles a tie.
    const QString language = QLocale().name().section(QLatin1Char('_'), 0, 0);
    QString best;
    int32_t bestConfidence = 0;
    bool bestInLanguage = false;
    for (int32_t i = 0; i < count; ++i) {
        status = U_ZERO_ERROR;
        const QString name = QString::fromLatin1(ucsdet_getName(matches[i], &status));
        const int32_t confidence = ucsdet_getConfidence(matches[i], &status);
        const bool inLanguage = language == QLatin1String(ucsdet_getLanguage(matches[i], &status));
        // Without a byte order mark, NUL bytes already decided UTF-16 and UTF-32.
        if (U_FAILURE(status) || name.startsWith(QLatin1String("UTF-"))
            || !QStringDecoder(name).isValid())
            continue;
        if (confidence > bestConfidence
            || (confidence == bestConfidence && inLanguage && !bestInLanguage)) {
            best = name;
            bestConfidence = confidence;
            bestInLanguage = inLanguage;
        }
    }
    if (best.isEmpty())
        return {};
    return QStringDecoder(best);
}

// The start of a text file, or nothing for content that is not text.
static std::optional<QString> readText(const QString &path, bool *truncated)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = file.read(textLimit + 1);
    *truncated = data.size() > textLimit;
    if (*truncated) {
        data.truncate(textLimit);
        // Ends on a whole line where there is one.
        const qsizetype end = data.lastIndexOf('\n');
        if (end > 0)
            data.truncate(end + 1);
    }
    const auto encoding = QStringConverter::encodingForData(data);
    // Only UTF-16 and UTF-32 text holds NUL bytes.
    if (!encoding && data.contains('\0'))
        return {};
    // A character cut at the limit stays in the decoder's state.
    QStringDecoder decoder(encoding.value_or(QStringConverter::Utf8));
    QString text = decoder.decode(data);
    if (!encoding && decoder.hasError()) {
        if (QStringDecoder legacy = legacyDecoder(data); legacy.isValid())
            text = legacy.decode(data);
    }
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

static QVariantMap describeFile(const QUrl &url, const QFont &font, qreal textWidth)
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
    } else if (mime.inherits("text/plain")) {
        bool truncated = false;
        auto text = readText(info.filePath(), &truncated);
        if (text) {
            result.insert("kind", "text");
            result.insert("breaks", splitLongParagraphs(&*text, font, textWidth));
            result.insert("text", *text);
            result.insert("truncated", truncated);
        }
    }

    return result;
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

QFont PreviewController::fixedFont() const
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

void PreviewController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
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
    watcher->setFuture(QtConcurrent::run(describeFile, url, fixedFont(), m_textWidth));
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
