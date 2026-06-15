import QtQuick
import ArchipelagoBackend

Rectangle {
    id: root

    property var shellWindow: null
    property string moduleId: ""
    property bool mounted: false
    property bool opened: false
    property bool contentVisible: false
    property bool geometryAnimationEnabled: false
    property bool hovered: surfaceArea.containsMouse
    property real originX: 0
    property real originY: 0
    property real originWidth: ArchipelagoConfig.islandHeight
    property real originHeight: ArchipelagoConfig.islandHeight
    readonly property real targetWidth: calculateTargetWidth()
    readonly property real targetHeight: calculateTargetHeight()
    readonly property real targetX: clamp(originX + originWidth / 2 - targetWidth / 2,
                                          ArchipelagoConfig.edgeMargin,
                                          Math.max(ArchipelagoConfig.edgeMargin, parent.width - ArchipelagoConfig.edgeMargin - targetWidth))
    readonly property real targetY: originY + ArchipelagoConfig.islandHeight + ArchipelagoConfig.expandedVerticalOffset
    readonly property real maskX: mounted ? Math.min(originX, targetX) : 0
    readonly property real maskY: mounted ? Math.min(originY, targetY) : 0
    readonly property real maskWidth: mounted ? Math.max(originX + originWidth, targetX + targetWidth) - maskX : 0
    readonly property real maskHeight: mounted ? Math.max(originY + originHeight, targetY + targetHeight) - maskY : 0

    signal closed()

    x: opened ? targetX : originX
    y: opened ? targetY : originY
    width: opened ? targetWidth : originWidth
    height: opened ? targetHeight : originHeight
    radius: opened ? StyleTokens.radiusPanel : originHeight / 2
    opacity: mounted ? 1 : 0
    visible: mounted || opacity > 0
    color: StyleTokens.panel
    border.width: opened ? 1 : 0
    border.color: StyleTokens.overviewBorder
    clip: true
    antialiasing: true

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    function calculateTargetWidth() {
        const base = shellWindow ? shellWindow.preferredExpandedWidth(moduleId) : ArchipelagoConfig.expandedWidth;
        return Math.min(base, Math.max(320, parent.width - ArchipelagoConfig.edgeMargin * 2));
    }

    function calculateTargetHeight() {
        const base = shellWindow ? shellWindow.preferredExpandedHeight(moduleId) : ArchipelagoConfig.expandedHeight;
        const screenHeight = shellWindow && shellWindow.screen ? shellWindow.screen.height : 900;
        const maximum = Math.max(220, screenHeight - targetY - ArchipelagoConfig.edgeMargin);
        return Math.min(base, maximum);
    }

    function openModule(nextModuleId, originRect) {
        cleanupTimer.stop();
        revealTimer.stop();
        geometryAnimationEnabled = false;
        moduleId = nextModuleId;
        originX = originRect.x;
        originY = originRect.y;
        originWidth = originRect.width;
        originHeight = originRect.height;
        contentVisible = false;
        mounted = true;
        opened = false;
        Qt.callLater(function() {
            if (!root.mounted)
                return;
            root.geometryAnimationEnabled = true;
            root.opened = true;
            revealTimer.restart();
        });
    }

    function close() {
        if (!mounted)
            return;
        revealTimer.stop();
        contentVisible = false;
        geometryAnimationEnabled = true;
        opened = false;
        cleanupTimer.restart();
    }

    Behavior on x {
        enabled: root.geometryAnimationEnabled
        NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutQuint }
    }
    Behavior on y {
        enabled: root.geometryAnimationEnabled
        NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutQuint }
    }
    Behavior on width {
        enabled: root.geometryAnimationEnabled
        NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutQuint }
    }
    Behavior on height {
        enabled: root.geometryAnimationEnabled
        NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutQuint }
    }
    Behavior on radius {
        enabled: root.geometryAnimationEnabled
        NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutQuint }
    }
    Behavior on opacity {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    Timer {
        id: revealTimer

        interval: Math.max(120, Math.round(ArchipelagoConfig.morphDuration * 0.45))
        repeat: false
        onTriggered: root.contentVisible = true
    }

    Timer {
        id: cleanupTimer

        interval: ArchipelagoConfig.morphDuration + 40
        repeat: false
        onTriggered: {
            root.geometryAnimationEnabled = false;
            root.mounted = false;
            root.moduleId = "";
            root.closed();
        }
    }

    MouseArea {
        id: surfaceArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.RightButton
        onClicked: root.close()
    }

    Loader {
        id: contentLoader

        anchors.fill: parent
        active: root.mounted
        opacity: root.contentVisible ? 1 : 0
        sourceComponent: shellWindow ? shellWindow.expandedComponentFor(root.moduleId) : null

        Behavior on opacity {
            NumberAnimation {
                duration: ArchipelagoConfig.contentFadeDuration
                easing.type: Easing.OutCubic
            }
        }
    }
}
