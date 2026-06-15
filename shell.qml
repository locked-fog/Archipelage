import QtQuick
import Quickshell
import Quickshell.Io
import ArchipelagoBackend
import "qml"

Scope {
    id: shellRoot

    property bool shuttingDown: false

    function forEachShell(callback) {
        const windows = panelVariants.instances ? panelVariants.instances : [];
        for (let index = 0; index < windows.length; index++) {
            const window = windows[index];
            if (window)
                callback(window);
        }
    }

    function forFocusedShell(callback) {
        const windows = panelVariants.instances ? panelVariants.instances : [];
        for (let index = 0; index < windows.length; index++) {
            const window = windows[index];
            if (window && window.screenFocused) {
                callback(window);
                return;
            }
        }
        if (windows.length > 0 && windows[0])
            callback(windows[0]);
    }

    function showNotificationAll(appName, summary, body) {
        forEachShell((window) => window.showNotification(appName, summary, body));
    }

    IpcHandler {
        target: "archipelago"

        function toggle(moduleId) {
            shellRoot.forFocusedShell((window) => window.toggleModule(moduleId || "system"));
        }

        function close() {
            shellRoot.forEachShell((window) => window.closeExpanded());
        }

        function reloadConfig() {
            ArchipelagoConfig.reload();
        }

        function workspaceNext() {
            const niri = Compositor.niriService;
            if (niri)
                niri.focusWorkspaceRelative(1);
        }

        function workspacePrevious() {
            const niri = Compositor.niriService;
            if (niri)
                niri.focusWorkspaceRelative(-1);
        }
    }

    Connections {
        target: SystemServices

        function onNotificationReceived(appName, summary, body) {
            shellRoot.showNotificationAll(appName, summary, body);
        }
    }

    Component.onCompleted: {
        const niri = Compositor.niriService;
        if (niri)
            niri.requestRefresh();
        SystemServices.requestScreenRecordingSnapshot();
    }

    Component.onDestruction: {
        shuttingDown = true;
    }

    Variants {
        id: panelVariants

        model: Quickshell.screens

        ArchipelagoShell {
            required property var modelData

            screen: modelData
            shellRootController: shellRoot
        }
    }
}
