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
    property bool closing: false
    property bool collapsed: false
    property bool collapseFadeVisible: true
    property bool hovered: surfaceArea.containsMouse
    readonly property var morphPositionCurve: [0.16, 1, 0.3, 1, 1, 1]
    readonly property var morphSizeCurve: [0.22, 1, 0.36, 1, 1, 1]
    readonly property var fadeCurve: [0.33, 1, 0.68, 1, 1, 1]
    readonly property int collapseFadeDelay: 50
    readonly property int collapseFadeDuration: Math.max(120, Math.round(ArchipelagoConfig.morphDuration * 0.28))
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

    signal opening()
    signal closed()
    signal geometryChanged()

    x: opened ? targetX : originX
    y: opened ? targetY : originY
    width: opened ? targetWidth : originWidth
    height: opened ? targetHeight : originHeight
    radius: opened ? StyleTokens.radiusPanel : originHeight / 2
    opacity: mounted && collapseFadeVisible ? 1 : 0
    visible: mounted || opacity > 0
    color: StyleTokens.panel
    border.width: opened ? 1 : 0
    border.color: StyleTokens.overviewBorder
    clip: true
    antialiasing: true

    onMaskXChanged: geometryChanged()
    onMaskYChanged: geometryChanged()
    onMaskWidthChanged: geometryChanged()
    onMaskHeightChanged: geometryChanged()
    onMountedChanged: geometryChanged()
    onOpenedChanged: geometryChanged()

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
        collapseSettledTimer.stop();
        collapseFadeDelayTimer.stop();
        revealTimer.stop();
        geometryAnimationEnabled = false;
        closing = false;
        collapsed = false;
        collapseFadeVisible = true;
        moduleId = nextModuleId;
        originX = originRect.x;
        originY = originRect.y;
        originWidth = originRect.width;
        originHeight = originRect.height;
        contentVisible = false;
        mounted = true;
        opened = false;
        opening();
        Qt.callLater(function() {
            if (!root.mounted)
                return;
            root.geometryAnimationEnabled = true;
            root.opened = true;
            revealTimer.restart();
        });
    }

    function close() {
        if (!mounted || closing)
            return;
        revealTimer.stop();
        closing = true;
        collapsed = false;
        collapseFadeVisible = true;
        collapseSettledTimer.restart();
        contentVisible = false;
        geometryAnimationEnabled = true;
        opened = false;
        cleanupTimer.restart();
    }

    Behavior on x {
        enabled: root.geometryAnimationEnabled
        NumberAnimation {
            duration: ArchipelagoConfig.morphDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.morphPositionCurve
        }
    }
    Behavior on y {
        enabled: root.geometryAnimationEnabled
        NumberAnimation {
            duration: ArchipelagoConfig.morphDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.morphPositionCurve
        }
    }
    Behavior on width {
        enabled: root.geometryAnimationEnabled
        NumberAnimation {
            duration: ArchipelagoConfig.morphDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.morphSizeCurve
        }
    }
    Behavior on height {
        enabled: root.geometryAnimationEnabled
        NumberAnimation {
            duration: ArchipelagoConfig.morphDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.morphSizeCurve
        }
    }
    Behavior on radius {
        enabled: root.geometryAnimationEnabled
        NumberAnimation {
            duration: ArchipelagoConfig.morphDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.morphSizeCurve
        }
    }
    Behavior on opacity {
        NumberAnimation {
            duration: root.collapseFadeDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: root.fadeCurve
        }
    }

    Timer {
        id: revealTimer

        interval: Math.max(120, Math.round(ArchipelagoConfig.morphDuration * 0.45))
        repeat: false
        onTriggered: root.contentVisible = true
    }

    Timer {
        id: cleanupTimer

        interval: ArchipelagoConfig.morphDuration + root.collapseFadeDelay + root.collapseFadeDuration + 40
        repeat: false
        onTriggered: {
            root.geometryAnimationEnabled = false;
            root.mounted = false;
            root.moduleId = "";
            root.closed();
            root.closing = false;
            root.collapsed = false;
        }
    }

    Timer {
        id: collapseSettledTimer

        interval: ArchipelagoConfig.morphDuration
        repeat: false
        onTriggered: {
            root.collapsed = true;
            collapseFadeDelayTimer.restart();
        }
    }

    Timer {
        id: collapseFadeDelayTimer

        interval: root.collapseFadeDelay
        repeat: false
        onTriggered: root.collapseFadeVisible = false
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
                easing.type: Easing.BezierSpline
                easing.bezierCurve: root.fadeCurve
            }
        }

        Binding {
            target: contentLoader.item
            property: "shellWindow"
            value: root.shellWindow
            when: contentLoader.item && contentLoader.item.shellWindow !== undefined
            restoreMode: Binding.RestoreNone
        }
    }
}
