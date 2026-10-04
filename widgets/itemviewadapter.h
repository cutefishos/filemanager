#ifndef ITEMVIEWADAPTER_H
#define ITEMVIEWADAPTER_H

#include <QObject>

class ItemViewAdapter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject *adapterView READ adapterView WRITE setAdapterView NOTIFY adapterViewChanged)

public:
    explicit ItemViewAdapter(QObject *parent = nullptr);

    QObject *adapterView() const;
    void setAdapterView(QObject *view);

Q_SIGNALS:
    void adapterViewChanged() const;

private:
    QObject *m_adapterView;
};

#endif
