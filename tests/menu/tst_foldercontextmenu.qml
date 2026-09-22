import QtQuick
import QtTest
import "../../qml" as FM

Item {
    width: 400
    height: 300

    QtObject {
        id: folder
        property int selectionCount: 0
        property bool isDesktop: true
        property string invoked: ""
        readonly property var allApplications: [
            { name: "Editor", icon: "text-editor", desktopFile: "editor.desktop" },
            { name: "Viewer", icon: "document-open", desktopFile: "viewer.desktop" }
        ]
        property var applications: allApplications

        function action(name) {
            return {
                text: name === "openWith" ? "Open with" : name,
                visible: name === "newTextFile" || name === "openWith",
                enabled: true,
                trigger: function() { folder.invoked = name }
            }
        }
        function prepareContextMenu() {}
        function openWithApplications() { return applications }
        function openWithApplication(desktopFile) { invoked = desktopFile }
    }

    FM.FolderContextMenu {
        id: menu
        folderModel: folder
    }

    TestCase {
        name: "FolderContextMenu"
        when: windowShown

        function cleanup() {
            menu.dismissAllPopups()
            tryCompare(menu, "visible", false)
            folder.invoked = ""
        }

        function test_hover_and_activate_data() {
            return [
                { tag: "New Documents", selection: 0, row: "New Documents", index: 0, result: "newTextFile" },
                { tag: "Application", selection: 1, row: "Open with", index: 0, result: "editor.desktop" },
                { tag: "Other", selection: 1, row: "Open with", index: 2, result: "openWith" },
                { tag: "Single application", selection: 1, single: true, row: "Open with", index: 0, result: "editor.desktop" },
                { tag: "Single application Other", selection: 1, single: true, row: "Open with", index: 1, result: "openWith" }
            ]
        }

        function test_no_applications_opens_dialog() {
            folder.selectionCount = 1
            folder.applications = []
            menu.popupAt(100, 100)
            var row = menu.menuRows().filter(function(item) { return item.text === "Open with" })[0]
            verify(row !== undefined)
            verify(!row.hasChildMenu)
            menuTest.moveCursor(row.mapToGlobal(row.width / 2, row.height / 2))
            mouseMove(row, row.width / 2, row.height / 2)
            wait(250)
            mouseClick(row, row.width / 2, row.height / 2)
            tryCompare(folder, "invoked", "openWith")
            tryCompare(menu, "visible", false)
        }

        function test_hover_and_activate(data) {
            folder.selectionCount = data.selection
            folder.applications = data.single ? folder.allApplications.slice(0, 1) : folder.allApplications
            menu.popupAt(100, 100)
            var row = menu.menuRows().filter(function(item) { return item.text === data.row })[0]
            verify(row !== undefined)
            menuTest.moveCursor(row.mapToGlobal(row.width / 2, row.height / 2))
            mouseMove(row, row.width / 2, row.height / 2)
            var child = row.childMenu
            tryCompare(child, "visible", true)
            var target = child.menuRows()[data.index]
            verify(target !== undefined)
            if (data.selection === 1) {
                compare(child.menuRows()[0].text, "Editor")
                compare(child.menuRows()[0].icon.name, "text-editor")
                compare(child.menuRows().length, folder.applications.length + 1)
                compare(child.menuRows()[folder.applications.length].text, "Other")
                var rows = child.menuRows()
                var siblings = rows[0].parent.children
                for (var rowIndex = 0; rowIndex < rows.length - 1; ++rowIndex) {
                    var separators = 0
                    for (var i = 0; i < siblings.length; ++i) {
                        var sibling = siblings[i]
                        if (sibling.visible && sibling.height > 0 &&
                                sibling.y >= rows[rowIndex].y + rows[rowIndex].height &&
                                sibling.y < rows[rowIndex + 1].y)
                            separators++
                    }
                    compare(separators, rowIndex === rows.length - 2 ? 1 : 0)
                }
            }

            // A grabbed root receives the event even over its child window.
            var receiver = menu.pointerGrabEnabled ? menu.contentItem : child.contentItem
            var point = target.mapToItem(receiver, target.width / 2, target.height / 2)
            menuTest.moveCursor(target.mapToGlobal(target.width / 2, target.height / 2))
            mouseMove(receiver, point.x, point.y)
            wait(250)
            verify(child.visible, "Submenu closed during pointer handoff")
            compare(child.currentIndex, data.index)
            mouseClick(receiver, point.x, point.y)
            tryCompare(folder, "invoked", data.result)
            tryCompare(menu, "visible", false)
            tryCompare(child, "visible", false)
        }
    }
}
