import QtQuick
import ArchipelagoBackend

Item {
    id: root

    signal closeRequested()

    readonly property var niri: Compositor.niriService
    property int selectedWorkspaceIdx: niri ? niri.focusedWorkspaceIdx : 0
    readonly property var workspaces: sortedWorkspaces()
    readonly property var windows: niri ? niri.windowsForWorkspace(selectedWorkspaceIdx) : []

    function sortedWorkspaces() {
        if (!niri || !niri.workspaces)
            return [];
        const list = [];
        for (let index = 0; index < niri.workspaces.length; index++)
            list.push(niri.workspaces[index]);
        list.sort((a, b) => Number(a.idx || 0) - Number(b.idx || 0));
        return list;
    }

    function focusWorkspace(idx) {
        selectedWorkspaceIdx = idx;
        if (niri)
            niri.focusWorkspaceIdx(idx);
    }

    Connections {
        target: root.niri

        function onFocusedWorkspaceChanged() {
            root.selectedWorkspaceIdx = root.niri.focusedWorkspaceIdx;
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 18

        Text {
            id: title

            text: "Workspaces"
            color: StyleTokens.textPrimary
            font.pixelSize: ArchipelagoConfig.titleFontSize
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
        }

        Text {
            anchors.right: parent.right
            anchors.verticalCenter: title.verticalCenter
            text: root.niri && root.niri.active ? "niri" : "offline"
            color: root.niri && root.niri.active ? StyleTokens.success : StyleTokens.warning
            font.pixelSize: 12
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
        }

        Row {
            anchors.top: title.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 14

            Flickable {
                width: Math.min(210, parent.width * 0.34)
                height: parent.height
                contentHeight: workspaceColumn.height
                clip: true

                Column {
                    id: workspaceColumn

                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: root.workspaces

                        Rectangle {
                            required property var modelData

                            width: workspaceColumn.width
                            height: 42
                            radius: 18
                            color: Number(modelData.idx) === root.selectedWorkspaceIdx ? StyleTokens.moduleHover : StyleTokens.module
                            border.width: modelData.isFocused ? 1 : 0
                            border.color: StyleTokens.accent

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Text {
                                    width: 34
                                    text: String(modelData.displayName || modelData.idx)
                                    color: StyleTokens.textPrimary
                                    font.pixelSize: 14
                                    font.family: ArchipelagoConfig.textFontFamily
                                    font.weight: Font.DemiBold
                                    anchors.verticalCenter: parent.verticalCenter
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width - 78
                                    text: modelData.output || ""
                                    color: StyleTokens.textSecondary
                                    font.pixelSize: 12
                                    font.family: ArchipelagoConfig.textFontFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: modelData.isFocused ? "*" : ""
                                    color: StyleTokens.accent
                                    font.pixelSize: 16
                                    font.family: ArchipelagoConfig.textFontFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.focusWorkspace(Number(modelData.idx))
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: 1
                height: parent.height
                color: StyleTokens.track
            }

            Flickable {
                width: Math.max(220, parent.width - 225)
                height: parent.height
                contentHeight: windowColumn.height
                clip: true

                Column {
                    id: windowColumn

                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: root.windows

                        Rectangle {
                            required property var modelData

                            width: windowColumn.width
                            height: 48
                            radius: 18
                            color: modelData.isFocused ? StyleTokens.moduleHover : StyleTokens.module

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                onClicked: if (root.niri) root.niri.focusWindow(Number(modelData.id))
                            }

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8
                                spacing: 10
                                z: 1

                                Column {
                                    width: parent.width - 102
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1

                                    Text {
                                        width: parent.width
                                        text: modelData.title || modelData.appId || "Window"
                                        color: StyleTokens.textPrimary
                                        font.pixelSize: 13
                                        font.family: ArchipelagoConfig.textFontFamily
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: parent.width
                                        text: modelData.appId || ""
                                        color: StyleTokens.textSecondary
                                        font.pixelSize: 11
                                        font.family: ArchipelagoConfig.textFontFamily
                                        elide: Text.ElideRight
                                    }
                                }

                                Text {
                                    width: 36
                                    text: "Move"
                                    color: StyleTokens.accent
                                    font.pixelSize: 11
                                    font.family: ArchipelagoConfig.textFontFamily
                                    anchors.verticalCenter: parent.verticalCenter

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton
                                        onClicked: if (root.niri) root.niri.moveFocusedWindowToWorkspace(root.selectedWorkspaceIdx, true)
                                    }
                                }

                                Text {
                                    width: 34
                                    text: "Close"
                                    color: StyleTokens.danger
                                    font.pixelSize: 11
                                    font.family: ArchipelagoConfig.textFontFamily
                                    anchors.verticalCenter: parent.verticalCenter

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton
                                        onClicked: if (root.niri) root.niri.closeWindow(Number(modelData.id))
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: root.windows.length === 0 ? "No windows" : ""
                        color: StyleTokens.textSecondary
                        font.pixelSize: 13
                        font.family: ArchipelagoConfig.textFontFamily
                        horizontalAlignment: Text.AlignHCenter
                        visible: root.windows.length === 0
                    }
                }
            }
        }
    }
}
