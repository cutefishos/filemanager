#include "pathcompletionmodel.h"

#include <QCollator>
#include <QDir>
#include <QUrl>

#include <algorithm>

PathCompletionModel::PathCompletionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QString PathCompletionModel::text() const
{
    return m_text;
}

void PathCompletionModel::setText(const QString &text)
{
    if (m_text == text)
        return;

    m_text = text;
    update();
    emit textChanged();
}

int PathCompletionModel::count() const
{
    return m_matches.size();
}

QString PathCompletionModel::commonCompletion() const
{
    if (m_matches.isEmpty())
        return QString();

    const QString &first = m_matches.first();
    int length = first.size();

    for (const QString &match : m_matches) {
        int i = 0;
        while (i < length && i < match.size() && first.at(i).toCaseFolded() == match.at(i).toCaseFolded())
            ++i;
        length = i;
    }

    return m_typedDir + first.left(length);
}

QString PathCompletionModel::completion(int row) const
{
    if (row < 0 || row >= m_matches.size())
        return QString();

    return m_typedDir + m_matches.at(row);
}

QString PathCompletionModel::path(int row) const
{
    if (row < 0 || row >= m_matches.size())
        return QString();

    return QDir::cleanPath(m_dir + QLatin1Char('/') + m_matches.at(row));
}

QHash<int, QByteArray> PathCompletionModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "name";
    roleNames[PathRole] = "path";
    roleNames[CompletionRole] = "completion";
    return roleNames;
}

int PathCompletionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_matches.size();
}

QVariant PathCompletionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_matches.size())
        return QVariant();

    switch (role) {
    case NameRole:
        return m_matches.at(index.row());
    case PathRole:
        return path(index.row());
    case CompletionRole:
        return completion(index.row());
    }

    return QVariant();
}

void PathCompletionModel::update()
{
    beginResetModel();

    m_matches.clear();

    const int slash = m_text.lastIndexOf(QLatin1Char('/'));
    m_typedDir = m_text.left(slash + 1);
    const QString prefix = m_text.mid(slash + 1);

    if (m_typedDir.startsWith(QLatin1String("~/")))
        m_dir = QDir::homePath() + m_typedDir.mid(1);
    else if (m_typedDir.startsWith(QLatin1String("file://")))
        m_dir = QUrl(m_typedDir).toLocalFile();
    else if (m_typedDir.startsWith(QLatin1Char('/')))
        m_dir = m_typedDir;
    else
        m_dir.clear();

    // Folders made since the editor opened show up the next time it does.
    if (m_dir.isEmpty())
        m_listedDir.clear();

    if (!m_dir.isEmpty()) {
        // Every keystroke refilters; only a new directory part lists again.
        if (m_dir != m_listedDir) {
            m_entries = QDir(m_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);

            QCollator collator;
            collator.setNumericMode(true);
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            std::sort(m_entries.begin(), m_entries.end(), collator);

            m_listedDir = m_dir;
        }

        // Hidden folders only once a dot asks for them.
        const bool hidden = prefix.startsWith(QLatin1Char('.'));
        for (const QString &entry : std::as_const(m_entries)) {
            if (entry.startsWith(QLatin1Char('.')) != hidden)
                continue;
            if (entry.startsWith(prefix, Qt::CaseInsensitive))
                m_matches.append(entry);
        }

        // Nothing left to complete once the name is typed out.
        if (m_matches.size() == 1 && m_matches.first() == prefix)
            m_matches.clear();
    }

    endResetModel();
    emit countChanged();
}
