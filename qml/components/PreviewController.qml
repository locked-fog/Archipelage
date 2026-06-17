import QtQuick
import ArchipelagoCore

Item {
    id: root

    property var shellWindow: null
    property var instances: []
    property var closingSurfaces: []
    property int closingCount: 0
    property int nextInstanceSerial: 0
    property string focusedInstanceId: ""
    property int geometryRevision: 0
    readonly property bool mounted: instances.length + closingSurfaces.length > 0
    readonly property bool hasFocusedPreview: focusedInstanceId !== ""
    readonly property real maskX: calculateMask("x", geometryRevision)
    readonly property real maskY: calculateMask("y", geometryRevision)
    readonly property real maskWidth: calculateMask("width", geometryRevision)
    readonly property real maskHeight: calculateMask("height", geometryRevision)

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    function layoutKey(pluginId, templateId) {
        return pluginId + "/" + templateId;
    }

    function templateFor(pluginId, templateId) {
        return shellWindow ? shellWindow.previewTemplateFor(pluginId, templateId) : null;
    }

    function normalizedOptions(options) {
        return options === undefined || options === null ? ({}) : options;
    }

    function bumpGeometryRevision() {
        geometryRevision = geometryRevision + 1;
    }

    function setInstances(next) {
        instances = next;
        bumpGeometryRevision();
    }

    function visibleInstancesFor(pluginId, templateId) {
        const key = layoutKey(pluginId, templateId);
        const result = [];
        for (let index = 0; index < instances.length; index++) {
            const item = instances[index];
            if (!item || item.closing || layoutKey(item.pluginId, item.templateId) !== key)
                continue;
            result.push(item);
        }
        return result;
    }

    function restack() {
        const counts = {};
        for (let index = 0; index < instances.length; index++) {
            const item = instances[index];
            if (!item || item.closing)
                continue;
            const key = layoutKey(item.pluginId, item.templateId);
            item.stackIndex = counts[key] || 0;
            counts[key] = item.stackIndex + 1;
        }
        bumpGeometryRevision();
    }

    function trimLayout(pluginId, templateId, maxVisible) {
        const visible = visibleInstancesFor(pluginId, templateId);
        for (let index = maxVisible; index < visible.length; index++)
            dismiss(visible[index].instanceId);
    }

    function clearFocusedSurface() {
        for (let index = 0; index < instances.length; index++) {
            const item = instances[index];
            if (item)
                item.focused = false;
        }
        focusedInstanceId = "";
    }

    function protectedOffsetFor(pluginId, templateId, templateHeight, wantsFocus) {
        if (focusedInstanceId === "" || wantsFocus)
            return 0;

        for (let index = 0; index < instances.length; index++) {
            const focused = instances[index];
            if (!focused || focused.instanceId !== focusedInstanceId)
                continue;
            if (layoutKey(focused.pluginId, focused.templateId) === layoutKey(pluginId, templateId))
                return 0;
            return Number(focused.previewHeight || templateHeight || 112) + 12;
        }
        return 0;
    }

    function show(pluginId, templateId, payload, options) {
        const template = templateFor(pluginId, templateId);
        if (!template || !template.component) {
            console.warn("[PreviewController] missing preview template", pluginId + "/" + templateId);
            return "";
        }

        const opts = normalizedOptions(options);
        const focusPolicy = opts.focusPolicy || template.focusPolicy || "passive";
        const wantsFocus = focusPolicy === "request";
        const previewWidth = Number(opts.width || template.defaultWidth || 360);
        const previewHeight = Number(opts.height || template.defaultHeight || 112);
        const instanceId = "preview-" + nextInstanceSerial;
        nextInstanceSerial = nextInstanceSerial + 1;
        const surface = previewSurfaceComponent.createObject(root, {
            "instanceId": instanceId,
            "pluginId": pluginId,
            "templateId": templateId,
            "payload": payload || {},
            "previewComponent": template.component,
            "originRect": opts.originRect || null,
            "previewWidth": previewWidth,
            "previewHeight": previewHeight,
            "timeoutMs": Number(opts.timeoutMs || template.timeoutMs || 5000),
            "maxVisible": Number(opts.maxVisible || template.maxVisible || 5),
            "focusPolicy": focusPolicy,
            "focused": wantsFocus,
            "activatedCallback": opts.onActivated || null,
            "dismissedCallback": opts.onDismissed || null,
            "protectedOffset": protectedOffsetFor(pluginId, templateId, previewHeight, wantsFocus),
            "stackIndex": 0
        });

        if (!surface) {
            console.warn("[PreviewController] failed to create preview surface", pluginId + "/" + templateId);
            return "";
        }

        if (wantsFocus)
            clearFocusedSurface();

        const next = instances.slice();
        next.unshift(surface);
        setInstances(next);
        if (wantsFocus)
            focusedInstanceId = instanceId;
        restack();
        trimLayout(pluginId, templateId, surface.maxVisible);
        return instanceId;
    }

    function dismiss(instanceId) {
        if (instanceId === "")
            return;
        for (let index = 0; index < instances.length; index++) {
            const item = instances[index];
            if (!item || item.instanceId !== instanceId)
                continue;
            const next = instances.slice();
            next.splice(index, 1);
            setInstances(next);
            const closing = closingSurfaces.slice();
            closing.push(item);
            closingSurfaces = closing;
            item.beginClose();
            closingCount = closingSurfaces.length;
            if (focusedInstanceId === instanceId)
                focusedInstanceId = "";
            restack();
            return;
        }
    }

    function dismissLayout(pluginId, templateId) {
        const visible = visibleInstancesFor(pluginId, templateId);
        for (let index = 0; index < visible.length; index++)
            dismiss(visible[index].instanceId);
    }

    function dismissAll() {
        const visible = instances.slice();
        for (let index = 0; index < visible.length; index++) {
            const item = visible[index];
            if (item && item.instanceId)
                dismiss(item.instanceId);
        }
    }

    function finishClose(surface) {
        const next = [];
        for (let index = 0; index < closingSurfaces.length; index++) {
            const item = closingSurfaces[index];
            if (item && item !== surface)
                next.push(item);
        }
        closingSurfaces = next;
        closingCount = closingSurfaces.length;
        if (surface)
            surface.destroy();
        bumpGeometryRevision();
    }

    function activate(instanceId) {
        for (let index = 0; index < instances.length; index++) {
            const item = instances[index];
            if (!item || item.instanceId !== instanceId)
                continue;
            if (typeof item.activatedCallback === "function")
                item.activatedCallback(item.payload);
            dismiss(instanceId);
            return;
        }
    }

    function fallbackOrigin() {
        return {
            "x": parent ? parent.width - ArchipelagoConfig.edgeMargin - ArchipelagoConfig.islandHeight : 0,
            "y": ArchipelagoConfig.edgeMargin,
            "width": ArchipelagoConfig.islandHeight,
            "height": ArchipelagoConfig.islandHeight
        };
    }

    function targetRectFor(instance) {
        const origin = instance.originRect || fallbackOrigin();
        const widthValue = Number(instance.previewWidth || 360);
        const heightValue = Number(instance.previewHeight || 112);
        const targetX = clamp(origin.x + origin.width - widthValue,
                              ArchipelagoConfig.edgeMargin,
                              Math.max(ArchipelagoConfig.edgeMargin,
                                       root.width - ArchipelagoConfig.edgeMargin - widthValue));
        const targetY = origin.y + origin.height + ArchipelagoConfig.expandedVerticalOffset
            + Number(instance.protectedOffset || 0)
            + Number(instance.stackIndex || 0) * (heightValue + 8);
        return {
            "x": targetX,
            "y": targetY,
            "width": widthValue,
            "height": heightValue
        };
    }

    function calculateMask(field, revision) {
        if (instances.length === 0 && closingSurfaces.length === 0)
            return 0;

        let minX = 1000000;
        let minY = 1000000;
        let maxX = 0;
        let maxY = 0;
        const tracked = instances.concat(closingSurfaces);
        for (let index = 0; index < tracked.length; index++) {
            const item = tracked[index];
            if (!item)
                continue;

            const origin = item.originRect || fallbackOrigin();
            const target = targetRectFor(item);
            minX = Math.min(minX, origin.x, target.x);
            minY = Math.min(minY, origin.y, target.y);
            maxX = Math.max(maxX, origin.x + origin.width, target.x + target.width);
            maxY = Math.max(maxY, origin.y + origin.height, target.y + target.height);
        }

        if (field === "x")
            return minX;
        if (field === "y")
            return minY;
        if (field === "width")
            return maxX - minX;
        return maxY - minY;
    }

    Component {
        id: previewSurfaceComponent

        Rectangle {
            id: surface

            property string instanceId: ""
            property string pluginId: ""
            property string templateId: ""
            property var payload: ({})
            property var previewComponent: null
            property var originRect: root.fallbackOrigin()
            property real previewWidth: 360
            property real previewHeight: 112
            property int timeoutMs: 5000
            property int maxVisible: 5
            property string focusPolicy: "passive"
            property bool focused: false
            property var activatedCallback: null
            property var dismissedCallback: null
            property real protectedOffset: 0
            property int stackIndex: 0
            property bool opened: false
            property bool closing: false
            property bool hovered: surfaceArea.containsMouse
            property var contentItem: null
            readonly property var targetRect: root.targetRectFor(surface)

            x: closing || !opened ? originRect.x : targetRect.x
            y: closing || !opened ? originRect.y : targetRect.y
            width: closing || !opened ? originRect.width : targetRect.width
            height: closing || !opened ? originRect.height : targetRect.height
            opacity: closing || !opened ? 0 : 1
            radius: closing || !opened ? height / 2 : StyleTokens.radiusModule
            color: StyleTokens.panel
            border.width: 1
            border.color: StyleTokens.overviewBorder
            clip: true
            antialiasing: true
            focus: focused
            z: 1000 - stackIndex

            function beginClose() {
                if (closing)
                    return;
                closing = true;
                if (typeof dismissedCallback === "function")
                    dismissedCallback(instanceId, pluginId, templateId);
                closeCleanupTimer.restart();
                root.bumpGeometryRevision();
            }

            function createContent() {
                if (!previewComponent || contentItem)
                    return;

                const item = previewComponent.createObject(contentHost);
                if (!item) {
                    console.warn("[PreviewController] preview content creation failed for", pluginId + "/" + templateId);
                    return;
                }

                contentItem = item;
                item.anchors.fill = contentHost;
                if (item.payload !== undefined)
                    item.payload = payload;
                if (item.instanceId !== undefined)
                    item.instanceId = instanceId;
                if (item.previewController !== undefined)
                    item.previewController = root;
                if (item.shellWindow !== undefined)
                    item.shellWindow = root.shellWindow;
            }

            function destroyContent() {
                if (!contentItem)
                    return;
                contentItem.destroy();
                contentItem = null;
            }

            onStackIndexChanged: root.bumpGeometryRevision()
            onOriginRectChanged: root.bumpGeometryRevision()
            onProtectedOffsetChanged: root.bumpGeometryRevision()

            Component.onCompleted: {
                createContent();
                Qt.callLater(function() {
                    if (surface.closing)
                        return;
                    surface.opened = true;
                    if (surface.focused)
                        surface.forceActiveFocus();
                    root.bumpGeometryRevision();
                });
            }

            Component.onDestruction: destroyContent()

            Behavior on x { NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutCubic } }
            Behavior on y { NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutCubic } }
            Behavior on height { NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutCubic } }
            Behavior on radius { NumberAnimation { duration: ArchipelagoConfig.morphDuration; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            Timer {
                interval: Math.max(0, surface.timeoutMs)
                running: surface.opened && !surface.closing && interval > 0 && !surface.hovered
                repeat: false
                onTriggered: root.dismiss(surface.instanceId)
            }

            Timer {
                id: closeCleanupTimer

                interval: ArchipelagoConfig.morphDuration + 40
                repeat: false
                onTriggered: root.finishClose(surface)
            }

            Item {
                id: contentHost

                anchors.fill: parent
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 3
                visible: surface.contentItem === null

                Text {
                    width: parent.width
                    text: surface.payload.sourceName || surface.pluginId
                    color: StyleTokens.textSecondary
                    font.pixelSize: 11
                    font.family: ArchipelagoConfig.textFontFamily
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    width: parent.width
                    text: surface.payload.title || surface.templateId
                    color: StyleTokens.textPrimary
                    font.pixelSize: 13
                    font.family: ArchipelagoConfig.textFontFamily
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    width: parent.width
                    text: surface.payload.body || ""
                    color: StyleTokens.textSecondary
                    font.pixelSize: 12
                    font.family: ArchipelagoConfig.textFontFamily
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    visible: text !== ""
                }
            }

            MouseArea {
                id: surfaceArea

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.RightButton
                onClicked: root.dismiss(surface.instanceId)
            }
        }
    }
}
