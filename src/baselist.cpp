/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2019  camilo <chiguitar@unal.edu.co>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "baselist.h"
#include "basemodel.h"

BaseList::BaseList(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
{
}

int BaseList::getCount() const
{
    return this->items().count();
}

QVariantMap BaseList::get(const int &index) const
{
    if (this->m_model) {
        return this->m_model->get(index);
    }

    if (index >= 0 && this->items().size() > 0 && index < this->items().size()) {
        return FMH::toMap(this->items()[index]);
    }

    return QVariantMap();
}

FMH::MODEL_LIST BaseList::getItems() const
{
    if (this->m_model && !this->m_model->getFilter().isEmpty()) {
        return FMH::toModelList(this->m_model->getAll());
    }

    return this->items();
}

int BaseList::mappedIndex(const int &index) const
{
    if (this->m_model)
        return this->m_model->mappedToSource(index);

    return index;
}

int BaseList::mappedIndexFromSource(const int &index) const
{
    if (this->m_model)
        return this->m_model->mappedFromSource(index);

    return index;
}

bool BaseList::exists(const FMH::MODEL_KEY &key, const QString &value) const
{
    return this->indexOf(key, value) >= 0;
}

int BaseList::indexOf(const FMH::MODEL_KEY &key, const QString &value) const
{
    const auto it = std::find_if(this->items().constBegin(), this->items().constEnd(), [&](const FMH::MODEL &item) -> bool {
        return item[key] == value;
    });

    if (it != this->items().constEnd())
        return this->mappedIndexFromSource(std::distance(this->items().constBegin(), it));
    else
        return -1;
}
