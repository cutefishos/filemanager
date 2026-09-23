import QtQuick

Window {
    width: 120
    height: 80
    visible: false

    function closeSelf() {
        close()
    }

    Item {
        objectName: "doomed"

        Item {}
    }
}
