#ifndef DESKTOPVIEW_H
#define DESKTOPVIEW_H

#include <QQuickView>

class DesktopView : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QRect screenRect READ screenRect NOTIFY screenRectChanged)
    Q_PROPERTY(QRect screenAvailableRect READ screenAvailableRect NOTIFY screenAvailableGeometryChanged)

public:
    explicit DesktopView(QQuickView *parent = nullptr);

    QRect screenRect();
    QRect screenAvailableRect();

signals:
    void screenRectChanged();
    void screenAvailableGeometryChanged();

private slots:
    void onGeometryChanged();
    void onAvailableGeometryChanged(const QRect &geometry);

private:
    QRect m_screenRect;
    QRect m_screenAvailableRect;
};

#endif // DESKTOPVIEW_H
