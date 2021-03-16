#ifndef RUBBERBAND_H
#define RUBBERBAND_H

#include <QQuickPaintedItem>

class RubberBand : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit RubberBand(QQuickItem *parent = nullptr);
    ~RubberBand() override;

    void paint(QPainter *painter) override;

    Q_INVOKABLE bool intersects(const QRectF &rect) const;

    QColor color() const;
    void setColor(QColor color);

signals:
    void colorChanged();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QRectF m_geometry;
    QColor m_color;
};

#endif
