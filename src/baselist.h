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

#ifndef BASELIST_H
#define BASELIST_H

#include "fmh.h"

#include <QQmlParserStatus>

/**
 * @todo write docs
 */
#include <QObject>

class BaseModel;
class BaseList : public QObject, public QQmlParserStatus
{
    Q_INTERFACES(QQmlParserStatus)

    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged FINAL)

public:
    /**
     * Default constructor
     */
    explicit BaseList(QObject *parent = nullptr);

    virtual const FMH::MODEL_LIST &items() const = 0;
    virtual void classBegin() override
    {
    }
    virtual void componentComplete() override
    {
    }
    virtual void modelHooked() {};

    int getCount() const;

    /**
     * @brief getItems
     * Get all the items in the list model. If the model has been filtered or sorted those are the items that are returned
     * @param index
     * @return
     */
    FMH::MODEL_LIST getItems() const;

    const BaseModel *m_model; // becarefull this is owned by qml engine, this is only supossed to be a viewer

public slots:
    int mappedIndex(const int &index) const;
    int mappedIndexFromSource(const int &index) const;
    QVariantMap get(const int &index) const;

protected:
    bool exists(const FMH::MODEL_KEY &key, const QString &value) const;
    int indexOf(const FMH::MODEL_KEY &key, const QString &value) const;

signals:
    void preItemAppended();
    void preItemsAppended(uint count);
    void postItemAppended();
    void preItemAppendedAt(int index);
    void preItemRemoved(int index);
    void postItemRemoved();
    void updateModel(int index, QVector<int> roles);
    void preListChanged();
    void postListChanged();

    void countChanged();
};

#endif // BASELIST_H
