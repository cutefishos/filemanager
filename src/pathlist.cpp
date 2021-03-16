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

#include "pathlist.h"

PathList::PathList(QObject *parent)
    : BaseList(parent)
{
}

QVariantMap PathList::get(const int &index) const
{
    if (this->list.isEmpty() || index >= this->list.size() || index < 0) {
        return QVariantMap();
    }

    const auto model = this->list.at(index);
    return FMH::toMap(model);
}

QString PathList::getPath() const
{
    return this->m_path;
}

const FMH::MODEL_LIST &PathList::items() const
{
    return this->list;
}

void PathList::setList()
{
    const auto paths = PathList::splitPath(m_path);

    if (this->list.isEmpty()) {
        emit this->preListChanged();
        this->list << paths;
        emit this->postListChanged();
    } else {
        const int index = [&]() -> int {
            int i = 0;
            for (const auto &item : qAsConst(list)) {
                if (i < paths.size()) {
                    if (item[FMH::MODEL_KEY::PATH] != paths[i][FMH::MODEL_KEY::PATH]) {
                        break;
                    } else
                        i++;
                } else
                    break;
            }
            return i;
        }();

        for (auto i = this->list.size() - 1; i >= index; i--) {
            emit preItemRemoved(i);
            this->list.removeAt(i);
            emit postItemRemoved();
        }

        for (auto i = index; i < paths.size(); i++) {
            emit preItemAppended();
            this->list << paths[i];
            emit postItemAppended();
        }
    }
}

void PathList::setPath(const QString &path)
{
    if (path == this->m_path)
        return;

    this->m_path = path;
    this->setList();

    emit this->pathChanged();

    qDebug() << this->list;
}

FMH::MODEL_LIST PathList::splitPath(const QString &path)
{
    FMH::MODEL_LIST res;

    QString _url = path;

    while (_url.endsWith("/"))
        _url.chop(1);

    _url += "/";

    const auto count = _url.count("/");

    for (auto i = 0; i < count; i++) {
        _url = QString(_url).left(_url.lastIndexOf("/"));
        auto label = QString(_url).right(_url.length() - _url.lastIndexOf("/") - 1);

        if (label.isEmpty())
            continue;

        if (label.contains(":") && i == count - 1) // handle the protocol
        {
            res << FMH::MODEL {{FMH::MODEL_KEY::LABEL, "/"}, {FMH::MODEL_KEY::PATH, _url + "///"}};
            break;
        }

        res << FMH::MODEL {{FMH::MODEL_KEY::LABEL, label}, {FMH::MODEL_KEY::PATH, _url}};
    }
    std::reverse(res.begin(), res.end());
    return res;
}
