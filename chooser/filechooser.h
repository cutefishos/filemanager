#pragma once

#include "model/foldermodel.h"

#include <QJsonObject>
#include <QSortFilterProxyModel>

class FileChooser : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
    Q_PROPERTY(bool saveMode READ saveMode CONSTANT)
    Q_PROPERTY(bool directoryMode READ directoryMode CONSTANT)
    Q_PROPERTY(bool multiple READ multiple CONSTANT)
    Q_PROPERTY(QString directory READ directory NOTIFY locationChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString selectionSummary READ selectionSummary NOTIFY selectionChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList filters READ filters CONSTANT)
    Q_PROPERTY(QVariantList choices READ choices CONSTANT)
    Q_PROPERTY(int filterIndex READ filterIndex WRITE setFilterIndex NOTIFY filterIndexChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY locationChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY locationChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit FileChooser(const QJsonObject &request, QObject *parent = nullptr);
    QString title() const;
    QString acceptLabel() const;
    bool saveMode() const;
    bool directoryMode() const;
    bool multiple() const;
    QString directory() const;
    QString fileName() const;
    void setFileName(const QString &name);
    QString selectionSummary() const;
    QString error() const;
    QVariantList filters() const;
    QVariantList choices() const;
    int filterIndex() const;
    void setFilterIndex(int index);
    bool showHidden() const;
    void setShowHidden(bool show);
    bool canGoBack() const;
    bool canGoForward() const;
    bool busy() const;

    Q_INVOKABLE void navigate(const QString &path);
    Q_INVOKABLE void up();
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void select(int row, int modifiers = 0);
    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void setChoice(const QString &id, const QString &value);
    Q_INVOKABLE void newFolder(const QString &name);
    Q_INVOKABLE void accept(bool overwrite = false);
    Q_INVOKABLE void cancel();

signals:
    void locationChanged();
    void fileNameChanged();
    void selectionChanged();
    void errorChanged();
    void filterIndexChanged();
    void showHiddenChanged();
    void busyChanged();
    void confirmOverwrite(const QString &name);
    void finished(const QJsonObject &result);

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    void setError(const QString &message);
    void complete(const QStringList &paths);
    bool validName(const QString &name) const;

    QJsonObject m_request;
    FolderModel m_folder;
    QString m_fileName;
    QString m_error;
    QString m_overwritePath;
    QJsonObject m_choices;
    int m_filterIndex = 0;
    int m_anchor = -1;
    bool m_finished = false;
};
