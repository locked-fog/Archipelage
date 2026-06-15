import QtQuick
import ArchipelagoBackend

Rectangle {
    id: root

    property string label: ""
    property bool emphasized: false

    signal clicked()

    width: emphasized ? 82 : 64
    height: 36
    radius: 18
    color: emphasized ? StyleTokens.textPrimary : StyleTokens.module
    opacity: enabled ? 1 : 0.45
    scale: control.pressed ? 0.94 : 1

    Behavior on scale {
        NumberAnimation { duration: 100 }
    }

    Text {
        anchors.centerIn: parent
        text: root.label
        color: root.emphasized ? StyleTokens.black : StyleTokens.textPrimary
        font.pixelSize: 12
        font.family: ArchipelagoConfig.textFontFamily
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: control

        anchors.fill: parent
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
