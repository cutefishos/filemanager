import QtQuick

Window {
    readonly property int loaderStatus: loader.status

    width: 120
    height: 80
    visible: false

    Loader {
        id: loader
        asynchronous: true
        sourceComponent: Item {}
    }
}
