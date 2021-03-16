#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QDBusInterface>

class DesktopSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString wallpaper READ wallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(bool dimsWallpaper READ dimsWallpaper NOTIFY dimsWallpaperChanged)

public:
    explicit DesktopSettings(QObject *parent = nullptr);

    QString wallpaper() const;
    bool dimsWallpaper() const;

    Q_INVOKABLE void launch(const QString &command, const QStringList &args);

signals:
    void wallpaperChanged();
    void dimsWallpaperChanged();

private slots:
    void init();
    void onWallpaperChanged(QString);

private:
    QDBusInterface m_interface;
    QString m_wallpaper;
};

#endif // SETTINGS_H
