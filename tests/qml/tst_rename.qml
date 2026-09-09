import QtQuick
import QtTest
import "../../qml"

TestCase {
    id: test
    name: "RenameEditor"
    when: windowShown
    visible: true
    width: 480
    height: 300
    Item {
        id: folderView
        anchors.fill: parent
        Item {
            id: item
            property alias labelArea: label
            Text { id: label; x: 50; y: 50; width: 160; text: "report.txt" }
        }
    }
    Component {
        id: factory
        RenameEditor {
            view: folderView
            targetItem: item
            originalName: "report.txt"
            selectionEnd: 6
        }
    }
    SignalSpy { id: submitted; signalName: "submitted" }
    SignalSpy { id: finished; signalName: "finished" }
    property var editor
    function init() {
        item.x = 0
        label.x = 50
        label.width = 160
        editor = factory.createObject(folderView)
        verify(editor !== null)
        submitted.target = editor
        finished.target = editor
        submitted.clear()
        finished.clear()
        editor.begin()
    }
    function cleanup() {
        editor.cancel()
        editor.destroy()
    }
    function test_selection_cancel() {
        compare(editor.selectedText, "report")
        compare(label.visible, false)
        keyClick(Qt.Key_Escape)
        compare(submitted.count, 0)
        compare(finished.count, 1)
        compare(label.visible, true)
        editor.cancel()
        compare(finished.count, 1)
    }
    function test_commit() {
        editor.text = "updated.txt"
        keyClick(Qt.Key_Return)
        compare(submitted.count, 1)
        compare(submitted.signalArguments[0][0], "updated.txt")
        compare(finished.count, 1)
    }
    function test_unchanged() {
        verify(editor.commit())
        compare(submitted.count, 0)
        compare(finished.count, 1)
    }
    function test_centered_geometry_data() {
        return [{tag: "right-column", x: 390, width: 84},
                {tag: "left-column", x: 6, width: 84},
                {tag: "right-edge", x: 396, width: 84},
                {tag: "left-edge", x: 0, width: 84}]
    }
    function test_centered_geometry(data) {
        editor.centered = true
        item.x = data.x
        label.x = 0
        label.width = data.width
        var center = label.mapToItem(folderView, label.width / 2, 0).x
        compare(editor.x + editor.width / 2, center)
        verify(editor.x >= 4)
        verify(editor.x + editor.width <= folderView.width - 4)
        verify(editor.width <= label.width + editor.leftPadding + editor.rightPadding)
        var originalHeight = editor.height
        editor.text = "A long desktop filename that should wrap below its own icon.txt"
        verify(editor.height > originalHeight)
        compare(editor.x + editor.width / 2, center)
    }
    function test_invalid_data() {
        return [{tag:"empty", name:""}, {tag:"spaces", name:"  "},
                {tag:"dot", name:"."}, {tag:"parent", name:".."},
                {tag:"slash", name:"a/b"}, {tag:"newline", name:"a\nb"}]
    }
    function test_invalid(data) {
        editor.text = data.name
        verify(!editor.commit())
        verify(editor.errorMessage.length > 0)
        compare(finished.count, 0)
        compare(submitted.count, 0)
        editor.text = "valid.txt"
        compare(editor.errorMessage, "")
        verify(editor.commit())
    }
}
