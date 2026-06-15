import QtQuick
import ArchipelagoBackend

Rectangle {
    id: root

    signal interactionStarted()
    signal valueMoved(real value)
    signal commitRequested()
    signal cancelRequested()

    property string title: ""
    property string valueText: ""
    property real value: 0

    function clamp01(nextValue) {
        return Math.max(0, Math.min(1, nextValue));
    }

    radius: StyleTokens.radiusModule
    color: sliderArea.containsMouse ? StyleTokens.moduleHover : StyleTokens.module

    Behavior on color {
        ColorAnimation { duration: 130 }
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.topMargin: 12
        text: root.title
        color: StyleTokens.textPrimary
        font.pixelSize: 13
        font.family: ArchipelagoConfig.textFontFamily
        font.weight: Font.DemiBold
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 14
        anchors.topMargin: 12
        text: root.valueText
        color: StyleTokens.textSecondary
        font.pixelSize: 12
        font.family: ArchipelagoConfig.textFontFamily
    }

    Rectangle {
        id: track

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 14
        height: 24
        radius: 12
        color: StyleTokens.track
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.value <= 0.001 ? 0 : Math.max(24, parent.width * root.clamp01(root.value))
            radius: parent.radius
            color: StyleTokens.textPrimary
        }

        Rectangle {
            x: Math.max(0, Math.min(parent.width - width, parent.width * root.clamp01(root.value) - width / 2))
            y: -1
            width: 26
            height: 26
            radius: 13
            color: StyleTokens.white
        }

        MouseArea {
            id: sliderArea

            anchors.fill: parent
            hoverEnabled: true

            function update(mouseX) {
                root.valueMoved(root.clamp01(mouseX / width));
            }

            onPressed: function(mouse) {
                root.interactionStarted();
                update(mouse.x);
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    update(mouse.x);
            }
            onReleased: root.commitRequested()
            onCanceled: root.cancelRequested()
        }
    }
}
