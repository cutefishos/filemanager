// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
    property bool selected: model.selected
    property bool blank: model.blank
    property bool isDir: model.isDir
    readonly property bool dropTarget: view && view.dropTargetItem === _listItem
    readonly property bool dropAllowed: dropTarget && view.dropTargetAllowed

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
        dropTarget: _listItem.dropTarget
        dropAllowed: _listItem.dropAllowed
        previousSelected: _listItem.previousSelected
        nextSelected: _listItem.nextSelected
    }
}
