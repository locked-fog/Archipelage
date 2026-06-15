import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: ""
    property var handlers: ({})
    readonly property var niri: Compositor.niriService
    readonly property var workspace: niri ? niri.focusedWorkspace : ({})
    readonly property string workspaceText: {
        if (!niri || !niri.active)
            return "niri";
        const name = workspace.displayName || workspace.name || workspace.idx || "";
        return String(name);
    }
    readonly property string outputText: workspace.output ? String(workspace.output).replace(/-.*/, "") : ""

    Component.onCompleted: {
        // wheel on the workspaces capsule focuses the relative workspace.
        // This is the only behaviour the workspaces compact view owns;
        // primary / secondary clicks fall through to the framework
        // default (host.activateModule → open the expanded surface).
        handlers = {
            wheelMoved: function(delta) {
                if (Compositor.niriService)
                    Compositor.niriService.focusWorkspaceRelative(delta > 0 ? -1 : 1);
            }
        };
    }

    Row {
        anchors.centerIn: parent
        spacing: 8

        Text {
            text: "WS"
            color: StyleTokens.textSecondary
            font.pixelSize: 11
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            visible: root.compactLevel < 2
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.workspaceText
            color: root.niri && root.niri.active ? StyleTokens.textPrimary : StyleTokens.warning
            font.pixelSize: root.compactLevel >= 2 ? 15 : 14
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
            width: root.compactLevel >= 2 ? 34 : 52
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            text: root.outputText
            color: StyleTokens.textMuted
            font.pixelSize: 11
            font.family: ArchipelagoConfig.textFontFamily
            visible: root.compactLevel === 0 && root.outputText !== ""
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
