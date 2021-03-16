#ifndef PLACESITEM_H
#define PLACESITEM_H

#include <QObject>
#include <QUrl>

class PlacesItem : public QObject
{
    Q_OBJECT

public:
    explicit PlacesItem(const QString &displayName = QString(),
                        const QString &iconName = QString(),
                        QUrl url = QUrl(),
                        QObject *parent = nullptr);

    QString displayName() const;
    void setDisplayName(const QString &name);

    QString iconName() const;
    void setIconName(const QString &name);

    QString iconPath() const;
    void setIconPath(const QString &path);

    QUrl url() const;
    void setUrl(const QUrl &url);

    QString path() const;

private:
    QString m_displayName;
    QString m_iconName;
    QString m_iconPath;
    QUrl m_url;
};

#endif // PLACESITEM_H
