#ifndef PATHCOMPLETIONMODEL_H
#define PATHCOMPLETIONMODEL_H

#include <QAbstractListModel>
#include <QStringList>

// The folders that complete a path being typed: the subfolders of its
// directory part whose names start with its last part.
class PathCompletionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // The text extended as far as every match agrees.
    Q_PROPERTY(QString commonCompletion READ commonCompletion NOTIFY countChanged)

public:
    enum DataRole {
        NameRole = Qt::UserRole + 1,
        PathRole,
        CompletionRole
    };

    explicit PathCompletionModel(QObject *parent = nullptr);

    QString text() const;
    void setText(const QString &text);

    int count() const;
    QString commonCompletion() const;

    // The typed text with row's folder name in place of its last part.
    Q_INVOKABLE QString completion(int row) const;
    Q_INVOKABLE QString path(int row) const;

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

signals:
    void textChanged();
    void countChanged();

private:
    void update();

    QString m_text;
    // What was typed up to and including the last slash, as typed, and the
    // local folder it names.
    QString m_typedDir;
    QString m_dir;
    // The subfolders of m_dir; kept while only the last part changes.
    QStringList m_entries;
    QString m_listedDir;
    QStringList m_matches;
};

#endif // PATHCOMPLETIONMODEL_H
