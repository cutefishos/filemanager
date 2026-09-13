/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick
import QtQuick.Controls
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0
import "FileList" as FileList

Item {
    id: _listItem

    readonly property ListView view: ListView.view

    width: view ? view.columns.availableWidth : 0
    height: view ? view.rowHeight : 0

    Accessible.name: model.fileName
    Accessible.role: Accessible.Row

    property Item iconArea: _row.iconArea
    property string editName: model.fileName
    property Item labelArea: _row.labelArea

    property int index: model.index
    property bool hovered: view && view.hoveredItem === _listItem
    property bool selected: model.selected
    property bool blank: model.blank

    readonly property bool previousSelected: {
        dirModel.selectionCount
        return selected && index > 0 && dirModel.isSelected(index - 1)
    }
    readonly property bool nextSelected: {
        dirModel.selectionCount
        return selected && view && index + 1 < view.count && dirModel.isSelected(index + 1)
    }

    onSelectedChanged: updateDragImage()
    onBlankChanged: updateDragImage()
    onXChanged: updateDragImage()
    onYChanged: updateDragImage()
    onWidthChanged: updateDragImage()
    onHeightChanged: updateDragImage()
    ListView.onReused: updateDragImage()
    Component.onCompleted: updateDragImage()

    function updateDragImage() {
        if (!view || !selected || blank || _listItem.width <= 0 || _listItem.height <= 0)
            return

        // The view snapshots the final selection when the rubber band closes.
        if (view.rubberBand)
            return

        var row = _listItem.index
        var x = _listItem.x
        var y = _listItem.y
        var width = _listItem.width
        var height = _listItem.height

        dirModel.beginItemDragImage(row)
        var grabbing = _listItem.grabToImage(function(result) {
            if (_listItem.selected && !_listItem.blank && _listItem.index === row)
                dirModel.addItemDragImage(row, x, y, width, height, result.image)
            else
                dirModel.cancelItemDragImage(row)
        })
        if (!grabbing)
            dirModel.cancelItemDragImage(row)
    }

    FileList.Row {
        id: _row
        anchors.fill: parent
        columns: _listItem.view.columns
        itemModel: model
        rowIndex: _listItem.index
        selected: _listItem.selected
        hovered: _listItem.hovered
        previousSelected: _listItem.previousSelected
        nextSelected: _listItem.nextSelected
    }
}
