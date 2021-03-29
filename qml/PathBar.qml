import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

Item {
    id: control

    property string url: ""

    signal itemClicked(string path)
    signal editorAccepted(string path)

    ListView {
        id: _pathView
        anchors.fill: parent
        model: _pathBarModel
        orientation: Qt.Horizontal
        layoutDirection: Qt.LeftToRight
        clip: true

        leftMargin: 3
        rightMargin: 3
        spacing: Meui.Units.smallSpacing

        onCountChanged: {
            _pathView.currentIndex = _pathView.count - 1
            _pathView.positionViewAtEnd()
        }

        Rectangle {
            anchors.fill: parent
            color: Meui.Theme.backgroundColor
            radius: Meui.Theme.smallRadius
            z: -1
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: openEditor()
            z: -1
        }

        delegate: MouseArea {
            id: _item
            height: ListView.view.height
            width: _name.width + Meui.Units.largeSpacing
            z: -1

            property bool selected: index === _pathView.count - 1

            onClicked: control.itemClicked(model.path)

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                anchors.bottomMargin: 2
                color: Meui.Theme.highlightColor
                radius: Meui.Theme.smallRadius
                visible: selected
            }

            Label {
                id: _name
                text: model.name
                color: selected ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
                anchors.centerIn: parent
            }
        }
    }

    TextField {
        id: _pathEditor
        anchors.fill: parent
        visible: false
        selectByMouse: true
        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase

        text: control.url

        onAccepted: {
            control.editorAccepted(text)
            closeEditor()
        }

        Keys.onPressed: {
            if (event.key === Qt.Key_Escape)
                focus = false
        }

        onActiveFocusChanged: {
            if (!activeFocus) {
                closeEditor()
            }
        }
    }

    PathBarModel {
        id: _pathBarModel
    }

    function updateUrl(url) {
        control.url = url
        _pathBarModel.url = url
    }

    function openEditor() {
        _pathEditor.text = control.url
        _pathEditor.visible = true
        _pathEditor.forceActiveFocus()
        _pathEditor.selectAll()
        _pathView.visible = false
    }

    function closeEditor() {
        _pathEditor.visible = false
        _pathView.visible = true
    }
}
