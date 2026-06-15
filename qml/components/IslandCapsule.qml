import QtQuick
import ArchipelagoBackend

Rectangle {
    id: root

    default property alias content: contentHost.data

    property bool interactive: true
    property color fillColor: StyleTokens.panel
    property color borderColor: StyleTokens.transparent

    signal primaryClicked()
    signal secondaryClicked()
    signal wheelMoved(int angleDelta)

    radius: height / 2
    color: fillColor
    border.width: borderColor === StyleTokens.transparent ? 0 : 1
    border.color: borderColor
    clip: true
    antialiasing: true

    Behavior on opacity {
        NumberAnimation {
            duration: ArchipelagoConfig.compactFadeDuration
            easing.type: Easing.OutCubic
        }
    }

    Behavior on color {
        ColorAnimation {
            duration: 130
        }
    }

    Item {
        id: contentHost

        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        opacity: root.opacity
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        enabled: root.interactive
        hoverEnabled: true

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton)
                root.secondaryClicked();
            else
                root.primaryClicked();
        }

        onWheel: function(wheel) {
            root.wheelMoved(wheel.angleDelta.y);
            wheel.accepted = true;
        }
    }
}
