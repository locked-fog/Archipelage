import QtQuick
import ArchipelagoCore

Item {
    id: root

    default property alias content: contentHost.data

    property bool interactive: true
    property color fillColor: StyleTokens.panel
    property color borderColor: StyleTokens.transparent
    property real revealOpacity: 1
    property real revealProgress: 1
    property real contentOpacity: 1
    readonly property var fadeCurve: [0.33, 1, 0.68, 1, 1, 1]
    readonly property real revealDotSize: Math.min(10, root.height)
    readonly property real revealSurfaceWidth: revealDotSize + Math.max(0, root.width - revealDotSize) * revealProgress
    readonly property real revealSurfaceHeight: revealDotSize + Math.max(0, root.height - revealDotSize) * revealProgress

    signal primaryClicked()
    signal secondaryClicked()
    signal wheelMoved(int angleDelta)
    signal pointerPressed(real x, real y, int button, int buttons)
    signal pointerMoved(real x, real y, int buttons)
    signal pointerReleased(real x, real y, int button, int buttons)
    signal pointerCanceled()

    Rectangle {
        id: surface

        width: root.revealSurfaceWidth
        height: root.revealSurfaceHeight
        anchors.centerIn: parent
        radius: height / 2
        color: root.fillColor
        opacity: root.revealOpacity
        border.width: root.borderColor === StyleTokens.transparent ? 0 : 1
        border.color: root.borderColor
        clip: true
        antialiasing: true

        Behavior on color {
            ColorAnimation {
                duration: 130
                easing.type: Easing.OutCubic
            }
        }
    }

    Item {
        id: contentHost

        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        opacity: root.contentOpacity
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        enabled: root.interactive
        hoverEnabled: true
        preventStealing: true

        onPressed: function(mouse) {
            root.pointerPressed(mouse.x, mouse.y, mouse.button, mouse.buttons);
        }

        onPositionChanged: function(mouse) {
            root.pointerMoved(mouse.x, mouse.y, mouse.buttons);
        }

        onReleased: function(mouse) {
            root.pointerReleased(mouse.x, mouse.y, mouse.button, mouse.buttons);
        }

        onCanceled: {
            root.pointerCanceled();
        }

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
