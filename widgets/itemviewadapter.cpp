#include "itemviewadapter.h"

ItemViewAdapter::ItemViewAdapter(QObject *parent)
    : QObject(parent)
    , m_adapterView(nullptr)
{
}

QObject *ItemViewAdapter::adapterView() const
{
    return m_adapterView;
}

void ItemViewAdapter::setAdapterView(QObject *view)
{
    if (m_adapterView != view) {
        m_adapterView = view;

        emit adapterViewChanged();
    }
}
