import QtQuick

Window {
    property bool allowClose: false

    width: 120
    height: 80
    visible: true

    onClosing: function(close) {
        close.accepted = allowClose
    }
}
