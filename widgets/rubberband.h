#ifndef RUBBERBAND_H
#define RUBBERBAND_H

#include <QColor>
#include <QQuickItem>

class RubberBand : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit RubberBand(QQuickItem *parent = nullptr);
    ~RubberBand() override;

    QColor color() const;
    void setColor(QColor color);

signals:
    void colorChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QColor m_color;
};

#endif
