import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import MeuiKit 1.0 as Meui

Dialog {
    id: control
    modal: true

    x: (parent.width - control.width) / 2
    y: (parent.height - control.height) / 2
}
