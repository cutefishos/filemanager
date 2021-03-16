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

#ifndef PATHLIST_H
#define PATHLIST_H

#include "baselist.h"

/**
 * @brief The PathList class
 */
class PathList : public BaseList
{
    Q_OBJECT

    Q_PROPERTY(QString path READ getPath WRITE setPath NOTIFY pathChanged)

public:
    PathList(QObject *parent = nullptr);

    const FMH::MODEL_LIST &items() const override;

    /**
     * @brief setPath
     * @param path
     */
    void setPath(const QString &path);

    /**
     * @brief getPath
     * @return
     */
    QString getPath() const;

    /**
     * @brief get
     * @param index
     * @return
     */
    QVariantMap get(const int &index) const;

private:
    FMH::MODEL_LIST list;
    QString m_path;

    static FMH::MODEL_LIST splitPath(const QString &path);
    void setList();

signals:
    /**
     * @brief pathChanged
     */
    void pathChanged();
};

#endif // PATHLIST_H
