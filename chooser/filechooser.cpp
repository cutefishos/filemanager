#include "filechooser.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

FileChooser::FileChooser(const QJsonObject &request, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_request(request)
{
    setSourceModel(&m_folder);
    m_folder.componentComplete();
    m_folder.setSortDirsFirst(true);
    connect(&m_folder, &FolderModel::selectionCountChanged, this, &FileChooser::selectionChanged);
    connect(&m_folder, &FolderModel::urlChanged, this, [this] {
        m_folder.clearSelection();
        m_anchor = -1;
        m_overwritePath.clear();
        emit locationChanged();
    });
    connect(&m_folder, &FolderModel::canGoBackChanged, this, &FileChooser::locationChanged);
    connect(&m_folder, &FolderModel::canGoForwardChanged, this, &FileChooser::locationChanged);
    connect(&m_folder, &FolderModel::statusChanged, this, &FileChooser::busyChanged);
    connect(&m_folder, &FolderModel::sortModeChanged, this, &FileChooser::sortChanged);
    connect(&m_folder, &FolderModel::sortDescChanged, this, &FileChooser::sortChanged);
    connect(&m_folder, &FolderModel::notification, this, &FileChooser::setError);

    m_filterIndex = qBound(0, request.value("filter_index").toInt(),
                           qMax(0, request.value("filters").toArray().size() - 1));
    for (const auto &entry : request.value("choices").toArray()) {
        const auto choice = entry.toObject();
        QString selected = choice.value("selected").toString();
        if (selected.isEmpty()) {
            const auto options = choice.value("options").toArray();
            selected =
                    options.isEmpty() ? "false" : options.first().toObject().value("id").toString();
        }
        m_choices.insert(choice.value("id").toString(), selected);
    }
    QString folder = request.value("current_folder").toString();
    m_fileName = request.value("current_name").toString();
    const QString currentFile = request.value("current_file").toString();
    if (saveMode() && !currentFile.isEmpty() && QFileInfo(currentFile).isAbsolute()) {
        folder = QFileInfo(currentFile).absolutePath();
        m_fileName = QFileInfo(currentFile).fileName();
    }
    if (!QFileInfo(folder).isAbsolute() || !QFileInfo(folder).isDir())
        folder = QDir::homePath();
    navigate(folder);
}

QString FileChooser::title() const
{
    return m_request.value("title").toString(saveMode() ? tr("Save File") : tr("Open"));
}

QString FileChooser::acceptLabel() const
{
    QString label = m_request.value("accept_label").toString();
    if (label.isEmpty())
        return m_request.value("mode").toString().startsWith("Save") ? tr("Save")
                : directoryMode()                                    ? tr("Select Folder")
                                                                     : tr("Open");
    return label.remove('_');
}

bool FileChooser::saveMode() const
{
    return m_request.value("mode").toString() == "SaveFile";
}

bool FileChooser::directoryMode() const
{
    return m_request.value("mode").toString() == "SaveFiles"
            || (!saveMode() && m_request.value("directory").toBool());
}

bool FileChooser::multiple() const
{
    return m_request.value("mode").toString() == "OpenFile" && m_request.value("multiple").toBool();
}

QString FileChooser::directory() const
{
    return m_folder.url();
}

QString FileChooser::fileName() const
{
    return m_fileName;
}

void FileChooser::setFileName(const QString &name)
{
    if (m_fileName == name)
        return;
    m_fileName = name;
    m_overwritePath.clear();
    emit fileNameChanged();
}

QString FileChooser::selectionSummary() const
{
    const auto urls = m_folder.selectedUrls();
    if (urls.size() == 1)
        return urls.first().fileName();
    if (urls.isEmpty())
        return directoryMode() ? directory() : tr("Select a file");
    return tr("%1 items selected").arg(urls.size());
}

QString FileChooser::error() const
{
    return m_error;
}

QVariantList FileChooser::filters() const
{
    return m_request.value("filters").toArray().toVariantList();
}

QVariantList FileChooser::choices() const
{
    return m_request.value("choices").toArray().toVariantList();
}

int FileChooser::filterIndex() const
{
    return m_filterIndex;
}

void FileChooser::setFilterIndex(int index)
{
    if (index == m_filterIndex || index < 0 || index >= filters().size())
        return;
    m_filterIndex = index;
    m_folder.clearSelection();
    invalidateFilter();
    emit filterIndexChanged();
}

bool FileChooser::showHidden() const
{
    return m_folder.showHiddenFiles();
}

void FileChooser::setShowHidden(bool show)
{
    m_folder.setShowHiddenFiles(show);
    emit showHiddenChanged();
}

bool FileChooser::canGoBack() const
{
    return m_folder.canGoBack();
}

bool FileChooser::canGoForward() const
{
    return m_folder.canGoForward();
}

bool FileChooser::busy() const
{
    return m_folder.status() == FolderModel::Listing;
}

int FileChooser::sortMode() const
{
    return m_folder.sortMode();
}

void FileChooser::setSortMode(int mode)
{
    m_folder.setSortMode(mode);
}

bool FileChooser::sortDesc() const
{
    return m_folder.sortDesc();
}

void FileChooser::setSortDesc(bool desc)
{
    m_folder.setSortDesc(desc);
}

void FileChooser::navigate(const QString &input)
{
    QUrl url(input);
    QString path = url.isLocalFile() ? url.toLocalFile() : input;
    if (path == "~" || path.startsWith("~/"))
        path.replace(0, 1, QDir::homePath());
    if (!QDir::isAbsolutePath(path))
        path = QDir(directory()).absoluteFilePath(path);
    const QFileInfo info(path);
    if (!info.isDir() || !info.isReadable()) {
        setError(tr("This folder is unavailable or cannot be read."));
        return;
    }
    setError({});
    m_folder.clearSelection();
    m_folder.setUrl(QDir::cleanPath(path));
}

void FileChooser::up()
{
    navigate(QFileInfo(directory()).dir().absolutePath());
}

void FileChooser::back()
{
    m_folder.goBack();
}

void FileChooser::forward()
{
    m_folder.goForward();
}

void FileChooser::select(int row, int modifiers)
{
    const auto item = index(row, 0);
    if (!item.isValid())
        return;
    const bool isDirectory = item.data(FolderModel::IsDirRole).toBool();
    const bool extend = multiple() && (modifiers & Qt::ShiftModifier) && m_anchor >= 0;
    if (extend) {
        m_folder.clearSelection();
        for (int i = qMin(m_anchor, row); i <= qMax(m_anchor, row); ++i)
            m_folder.setSelected(mapToSource(index(i, 0)).row());
    } else if (multiple() && (modifiers & Qt::ControlModifier)) {
        m_folder.toggleSelected(mapToSource(item).row());
        m_anchor = row;
    } else {
        m_folder.clearSelection();
        m_folder.setSelected(mapToSource(item).row());
        m_anchor = row;
    }
    if (saveMode() && !isDirectory)
        setFileName(item.data(FolderModel::FileNameRole).toString());
    setError({});
}

void FileChooser::activate(int row)
{
    const auto item = index(row, 0);
    if (!item.isValid())
        return;
    if (item.data(FolderModel::IsDirRole).toBool()) {
        navigate(item.data(FolderModel::UrlRole).toUrl().toLocalFile());
    } else if (!directoryMode()) {
        select(row);
        accept();
    }
}

void FileChooser::selectAll()
{
    if (!multiple())
        return;
    m_folder.clearSelection();
    for (int row = 0; row < rowCount(); ++row) {
        const auto item = index(row, 0);
        if (directoryMode() || !item.data(FolderModel::IsDirRole).toBool())
            m_folder.setSelected(mapToSource(item).row());
    }
}

bool FileChooser::isSelected(int row) const
{
    const auto item = index(row, 0);
    return item.isValid() && m_folder.isSelected(mapToSource(item).row());
}

void FileChooser::setChoice(const QString &id, const QString &value)
{
    m_choices.insert(id, value);
}

bool FileChooser::validName(const QString &name) const
{
    return !name.isEmpty() && name != "." && name != ".." && !name.contains('/')
            && !name.contains(QChar::Null);
}

void FileChooser::newFolder(const QString &name)
{
    if (!validName(name) || !QDir(directory()).mkdir(name)) {
        setError(tr("The folder could not be created. Choose another name."));
        return;
    }
    navigate(QDir(directory()).filePath(name));
}

void FileChooser::accept(bool overwrite)
{
    if (m_finished)
        return;
    setError({});
    QStringList paths;
    if (saveMode()) {
        if (!validName(m_fileName)) {
            setError(tr("Enter a file name without slashes."));
            return;
        }
        const QString target = QDir(directory()).absoluteFilePath(m_fileName);
        const QFileInfo info(target);
        if (info.isDir()) {
            navigate(target);
            return;
        }
        if (!QFileInfo(directory()).isWritable() || (info.exists() && !info.isWritable())) {
            setError(tr("You do not have permission to save here."));
            return;
        }
        if ((info.exists() || info.isSymLink()) && (!overwrite || m_overwritePath != target)) {
            m_overwritePath = target;
            emit confirmOverwrite(m_fileName);
            return;
        }
        paths.append(target);
    } else {
        const auto selected = m_folder.selectedUrls();
        for (const auto &url : selected) {
            if (!url.isLocalFile()) {
                setError(tr("Choose a local file or folder."));
                return;
            }
            const QFileInfo info(url.toLocalFile());
            if (!info.exists() || !info.isReadable()) {
                setError(tr("The selected item is no longer available."));
                return;
            }
            if (info.isDir() != directoryMode()) {
                if (!directoryMode() && selected.size() == 1)
                    navigate(info.absoluteFilePath());
                else
                    setError(directoryMode() ? tr("Select folders only.")
                                             : tr("Select files only."));
                return;
            }
            paths.append(info.absoluteFilePath());
        }
        if (paths.isEmpty() && directoryMode())
            paths.append(directory());
        if (paths.isEmpty()) {
            setError(tr("Select a file to open."));
            return;
        }
        if (!multiple() && paths.size() > 1)
            return;
    }
    if (m_request.value("mode").toString() == "SaveFiles") {
        const QString folder = paths.first();
        if (!QFileInfo(folder).isWritable()) {
            setError(tr("You do not have permission to save here."));
            return;
        }
        paths.clear();
        QSet<QString> used;
        for (const auto &entry : m_request.value("files").toArray()) {
            const QString name = entry.toString();
            if (!validName(name)) {
                setError(tr("A suggested file name is invalid."));
                return;
            }
            QString candidate = QDir(folder).absoluteFilePath(name);
            int suffix = 1;
            while (QFileInfo::exists(candidate) || QFileInfo(candidate).isSymLink()
                   || used.contains(candidate)) {
                const QFileInfo info(name);
                const QString extension = info.suffix().isEmpty() ? QString() : "." + info.suffix();
                candidate = QDir(folder).absoluteFilePath(info.completeBaseName()
                                                          + QStringLiteral(" (%1)").arg(suffix++)
                                                          + extension);
            }
            used.insert(candidate);
            paths.append(candidate);
        }
        if (paths.isEmpty()) {
            setError(tr("No file names were provided."));
            return;
        }
    }
    complete(paths);
}

void FileChooser::complete(const QStringList &paths)
{
    QJsonArray uris;
    for (const auto &path : paths)
        uris.append(QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded));
    m_finished = true;
    emit finished({{"response", 0},
                   {"uris", uris},
                   {"filter_index", m_filterIndex},
                   {"choices", m_choices}});
}

void FileChooser::cancel()
{
    if (m_finished)
        return;
    m_finished = true;
    emit finished({{"response", 1}});
}

void FileChooser::setError(const QString &message)
{
    m_error = message;
    emit errorChanged();
}

bool FileChooser::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const auto item = sourceModel()->index(row, 0, parent);
    if (item.data(FolderModel::IsDirRole).toBool())
        return true;
    if (directoryMode())
        return false;
    const auto allFilters = m_request.value("filters").toArray();
    if (allFilters.isEmpty())
        return true;
    const auto rules = allFilters.at(m_filterIndex).toObject().value("rules").toArray();
    const QString name = item.data(FolderModel::FileNameRole).toString();
    for (const auto &entry : rules) {
        const auto rule = entry.toObject();
        const QString value = rule.value("value").toString();
        if (rule.value("type").toInt() == 0) {
            const auto expression = QRegularExpression::fromWildcard(
                    value, Qt::CaseSensitive, QRegularExpression::NonPathWildcardConversion);
            if (expression.match(name).hasMatch())
                return true;
        } else if (rule.value("type").toInt() == 1) {
            const auto mime = QMimeDatabase().mimeTypeForFile(
                    item.data(FolderModel::UrlRole).toUrl().toLocalFile(),
                    QMimeDatabase::MatchExtension);
            if (mime.inherits(value)
                || (value.endsWith("/*") && mime.name().startsWith(value.chopped(1))))
                return true;
        }
    }
    return rules.isEmpty();
}
