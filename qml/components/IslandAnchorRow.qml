import QtQuick
import ArchipelagoCore

Item {
    id: root

    property string anchorName: ""
    property var host: null
    property var modules: []
    property int compactLevel: 0
    property int topMargin: 0

    signal moduleExitFinished(string moduleId, string anchorName)

    Repeater {
        model: root.modules

        IslandModule {
            required property string modelData

            anchorName: root.anchorName
            moduleId: modelData
            host: root.host
            compactLevel: root.compactLevel
            lifecycleActive: root.host ? root.host.isModuleActive(root.anchorName, modelData) : true
            x: {
                const layout = root.host ? root.host.layoutFor(modelData) : null;
                return layout ? Number(layout.targetX || 0) : 0;
            }
            y: root.topMargin

            onCompactExitFinished: function(moduleId, anchorName) {
                root.moduleExitFinished(moduleId, anchorName);
            }

            Behavior on x {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }

            Behavior on width {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }
        }
    }
}
