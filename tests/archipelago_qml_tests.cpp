#include <memory>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariant>

namespace {

class ArchipelagoQmlTests {
public:
    QQmlEngine m_engine;

    QString sourcePath(const QString &relativePath) const
    {
        return QStringLiteral(ARCHIPELAGO_SOURCE_DIR) + QLatin1Char('/') + relativePath;
    }

    QUrl sourceUrl(const QString &relativePath) const
    {
        return QUrl::fromLocalFile(sourcePath(relativePath));
    }

    QObject *createFromData(const QByteArray &qml)
    {
        QQmlComponent component(&m_engine);
        component.setData(qml, sourceUrl("tests/inline.qml"));
        if (component.isError())
            qWarning().noquote() << component.errorString();
        auto *object = component.create();
        if (!object)
            qWarning().noquote() << component.errorString();
        return object;
    }

    void initTestCase()
    {
        m_engine.addImportPath(QStringLiteral(ARCHIPELAGO_BUILD_DIR));
        m_engine.addImportPath(sourcePath("qml"));
    }

    bool timePluginComponentsLoad()
    {
        const QList<QString> pluginFiles = {
            QStringLiteral("qml/plugins/time/Compact.qml"),
            QStringLiteral("qml/plugins/time/Expanded.qml"),
        };

        for (const QString &pluginFile : pluginFiles) {
            QQmlComponent component(&m_engine, sourceUrl(pluginFile));
            if (component.isLoading()) {
                QSignalSpy spy(&component, &QQmlComponent::statusChanged);
                if (!spy.wait(1000))
                    return false;
            }
            if (component.isError())
                qWarning().noquote() << component.errorString();
            if (component.status() != QQmlComponent::Ready)
                return false;

            std::unique_ptr<QObject> object(component.create());
            if (!object) {
                qWarning().noquote() << component.errorString();
                return false;
            }
        }
        return true;
    }

    bool compactViewUpdatesWhenRegistryArrives()
    {
        const QByteArray qml = R"QML(
import QtQuick
import ArchipelagoCore
import "file:///)QML" ARCHIPELAGO_SOURCE_DIR R"QML(/qml/components"

Item {
    id: root

    width: 160
    height: 48

    property var moduleRegistry: ({})

    function moduleEntry(moduleId) {
        return moduleRegistry && moduleId ? (moduleRegistry[moduleId] || {}) : {};
    }

    function compactComponentFor(moduleId) {
        return moduleEntry(moduleId).compact || null;
    }

    function compactFor(moduleId) {
        return host.compactFor(moduleId);
    }

    function expandedComponentFor(moduleId) {
        return moduleEntry(moduleId).expanded || null;
    }

    function installCompact() {
        moduleRegistry = {
            "time": {
                "compact": realCompact,
                "expanded": null
            }
        };
    }

    function visibleText() {
        return findText(module);
    }

    function findText(item) {
        if (!item)
            return "";
        if (item.text !== undefined && item.text !== "")
            return item.text;
        const children = item.children || [];
        for (let index = 0; index < children.length; index++) {
            const text = findText(children[index]);
            if (text !== "")
                return text;
        }
        return "";
    }

    Component {
        id: realCompact

        Text {
            text: "loaded-time"
        }
    }

    Item {
        id: host

        property var shellWindow: root

        function compactFor(moduleId) {
            return parent && parent.compactComponentFor ? parent.compactComponentFor(moduleId) : null;
        }

        function expandedComponentFor(moduleId) {
            return parent && parent.expandedComponentFor ? parent.expandedComponentFor(moduleId) : null;
        }
    }

    IslandModule {
        id: module

        moduleId: "time"
        host: host
        compactLevel: 0
    }
}
)QML";

        std::unique_ptr<QObject> object(createFromData(qml));
        if (!object)
            return false;
        if (object->property("visibleText").toString() != QString())
            return false;

        QVariant initialText;
        if (!QMetaObject::invokeMethod(object.get(), "visibleText", Q_RETURN_ARG(QVariant, initialText)))
            return false;
        if (initialText.toString() != QStringLiteral("time")) {
            qWarning() << "Expected fallback text before registry update, got" << initialText.toString();
            return false;
        }

        if (!QMetaObject::invokeMethod(object.get(), "installCompact"))
            return false;
        QCoreApplication::processEvents();

        QVariant updatedText;
        if (!QMetaObject::invokeMethod(object.get(), "visibleText", Q_RETURN_ARG(QVariant, updatedText)))
            return false;
        if (updatedText.toString() != QStringLiteral("loaded-time")) {
            qWarning() << "Expected compact text after registry update, got" << updatedText.toString();
            return false;
        }
        return true;
    }

    bool islandHostUsesShellWindowForRegistry()
    {
        const QByteArray qml = R"QML(
import QtQuick
import ArchipelagoCore
import "file:///)QML" ARCHIPELAGO_SOURCE_DIR R"QML(/qml/components"

Item {
    id: root

    property var moduleRegistry: ({})

    function moduleEntry(moduleId) {
        return moduleRegistry && moduleId ? (moduleRegistry[moduleId] || {}) : {};
    }

    function compactComponentFor(moduleId) {
        return moduleEntry(moduleId).compact || null;
    }

    function expandedComponentFor(moduleId) {
        return moduleEntry(moduleId).expanded || null;
    }

    function installCompact() {
        moduleRegistry = {
            "time": {
                "compact": realCompact,
                "expanded": null
            }
        };
    }

    function hostHasCompact() {
        return host.compactFor("time") !== null;
    }

    Component {
        id: realCompact

        Text {
            text: "loaded-time"
        }
    }

    Item {
        id: visualParent
    }

    IslandHost {
        id: host

        parent: visualParent
        shellWindow: root
        width: 200
        height: 60
    }
}
)QML";

        std::unique_ptr<QObject> object(createFromData(qml));
        if (!object)
            return false;

        QVariant initial;
        if (!QMetaObject::invokeMethod(object.get(), "hostHasCompact", Q_RETURN_ARG(QVariant, initial)))
            return false;
        if (initial.toBool()) {
            qWarning() << "Expected IslandHost compact lookup to be empty before registry update";
            return false;
        }

        if (!QMetaObject::invokeMethod(object.get(), "installCompact"))
            return false;
        QCoreApplication::processEvents();

        QVariant updated;
        if (!QMetaObject::invokeMethod(object.get(), "hostHasCompact", Q_RETURN_ARG(QVariant, updated)))
            return false;
        if (!updated.toBool()) {
            qWarning() << "Expected IslandHost compact lookup to use shellWindow, not visual parent";
            return false;
        }
        return true;
    }

    bool dynamicLayoutHonorsSoftConfiguredWidthsAndRuntimePreferredWidths()
    {
        const QByteArray qml = R"QML(
import QtQuick
import ArchipelagoCore
import "file:///)QML" ARCHIPELAGO_SOURCE_DIR R"QML(/qml/components"

Item {
    id: root

    width: 260
    height: 72
    property bool betaWide: false
    property var moduleRegistry: ({
        "time": {
            "compact": timeCompact,
            "expanded": null,
            "anchors": ["center"],
            "defaultPriority": 50,
            "compactLayout": {
                "minimumWidth": 60,
                "maximumWidth": 180
            }
        },
        "beta": {
            "compact": betaCompact,
            "expanded": null,
            "anchors": ["center"],
            "defaultPriority": 60,
            "compactLayout": {
                "preferredWidth": 80,
                "minimumWidth": 60,
                "maximumWidth": 140
            }
        }
    })

    function moduleEntry(moduleId) {
        return moduleRegistry && moduleId ? (moduleRegistry[moduleId] || {}) : {};
    }

    function compactComponentFor(moduleId) {
        return moduleEntry(moduleId).compact || null;
    }

    function expandedComponentFor(moduleId) {
        return moduleEntry(moduleId).expanded || null;
    }

    function targetWidth(moduleId) {
        const layout = host.layoutFor(moduleId);
        return layout ? Number(layout.targetWidth || 0) : -1;
    }

    function shrink() {
        width = 182;
    }

    function widenBeta() {
        width = 260;
        betaWide = true;
    }

    Component {
        id: timeCompact

        Item {
            property int compactLevel: 0
            property string moduleId: ""
            property int minimumCompactWidth: 60
            property int maximumCompactWidth: 180

            Text {
                anchors.centerIn: parent
                text: "time"
            }
        }
    }

    Component {
        id: betaCompact

        Item {
            property int compactLevel: 0
            property string moduleId: ""
            property var shellWindow: null
            property int preferredCompactWidth: shellWindow && shellWindow.betaWide ? 120 : 80
            property int minimumCompactWidth: 60
            property int maximumCompactWidth: 140

            Text {
                anchors.centerIn: parent
                text: "beta"
            }
        }
    }

    IslandHost {
        id: host

        anchors.fill: parent
        shellWindow: root
    }
}
)QML";

        std::unique_ptr<QObject> object(createFromData(qml));
        if (!object)
            return false;

        QVariant initialTimeWidth;
        QVariant initialBetaWidth;
        if (!QMetaObject::invokeMethod(object.get(), "targetWidth", Q_RETURN_ARG(QVariant, initialTimeWidth),
                                       Q_ARG(QVariant, QStringLiteral("time"))))
            return false;
        if (!QMetaObject::invokeMethod(object.get(), "targetWidth", Q_RETURN_ARG(QVariant, initialBetaWidth),
                                       Q_ARG(QVariant, QStringLiteral("beta"))))
            return false;
        if (initialTimeWidth.toInt() != 108 || initialBetaWidth.toInt() != 80) {
            qWarning() << "Expected initial widths 108/80, got"
                       << initialTimeWidth.toInt() << initialBetaWidth.toInt();
            return false;
        }

        if (!QMetaObject::invokeMethod(object.get(), "shrink"))
            return false;
        QCoreApplication::processEvents();

        QVariant narrowedTimeWidth;
        QVariant narrowedBetaWidth;
        if (!QMetaObject::invokeMethod(object.get(), "targetWidth", Q_RETURN_ARG(QVariant, narrowedTimeWidth),
                                       Q_ARG(QVariant, QStringLiteral("time"))))
            return false;
        if (!QMetaObject::invokeMethod(object.get(), "targetWidth", Q_RETURN_ARG(QVariant, narrowedBetaWidth),
                                       Q_ARG(QVariant, QStringLiteral("beta"))))
            return false;
        if (narrowedTimeWidth.toInt() >= 108 || narrowedBetaWidth.toInt() != 80) {
            qWarning() << "Expected configured width to shrink under pressure and beta to keep 80, got"
                       << narrowedTimeWidth.toInt() << narrowedBetaWidth.toInt();
            return false;
        }

        if (!QMetaObject::invokeMethod(object.get(), "widenBeta"))
            return false;
        QCoreApplication::processEvents();

        QVariant widenedBetaWidth;
        if (!QMetaObject::invokeMethod(object.get(), "targetWidth", Q_RETURN_ARG(QVariant, widenedBetaWidth),
                                       Q_ARG(QVariant, QStringLiteral("beta"))))
            return false;
        if (widenedBetaWidth.toInt() != 120) {
            qWarning() << "Expected runtime preferred width update to reach 120, got"
                       << widenedBetaWidth.toInt();
            return false;
        }
        return true;
    }

    bool dynamicLayoutHonorsVisibilityRequestsAndPriorityHiding()
    {
        const QByteArray qml = R"QML(
import QtQuick
import ArchipelagoCore
import "file:///)QML" ARCHIPELAGO_SOURCE_DIR R"QML(/qml/components"

Item {
    id: root

    width: 420
    height: 72
    property bool showBeta: false
    property var moduleRegistry: ({
        "alpha": {
            "compact": alphaCompact,
            "expanded": null,
            "anchors": ["left"],
            "defaultPriority": 90,
            "compactLayout": {
                "preferredWidth": 70,
                "minimumWidth": 60,
                "maximumWidth": 90
            }
        },
        "beta": {
            "compact": betaCompact,
            "expanded": null,
            "anchors": ["left"],
            "defaultPriority": 60,
            "compactLayout": {
                "preferredWidth": 70,
                "minimumWidth": 60,
                "maximumWidth": 90
            }
        },
        "gamma": {
                "compact": gammaCompact,
                "expanded": null,
                "anchors": ["left"],
                "defaultPriority": 40,
                "compactLayout": {
                    "preferredWidth": 70,
                    "minimumWidth": 60,
                "maximumWidth": 90
            }
        }
    })

    function moduleEntry(moduleId) {
        return moduleRegistry && moduleId ? (moduleRegistry[moduleId] || {}) : {};
    }

    function compactComponentFor(moduleId) {
        return moduleEntry(moduleId).compact || null;
    }

    function expandedComponentFor(moduleId) {
        return moduleEntry(moduleId).expanded || null;
    }

    function enableBeta() {
        showBeta = true;
    }

    function betaVisible() {
        return host.layoutFor("beta").visible;
    }

    function betaReason() {
        return host.layoutFor("beta").collapseReason;
    }

    function alphaVisible() {
        return host.layoutFor("alpha").visible;
    }

    function gammaVisible() {
        return host.layoutFor("gamma").visible;
    }

    function gammaReason() {
        return host.layoutFor("gamma").collapseReason;
    }

    Component {
        id: alphaCompact

        Item {
            Text {
                anchors.centerIn: parent
                text: "alpha"
            }
        }
    }

    Component {
        id: betaCompact

        Item {
            property var shellWindow: null
            property bool compactVisibleRequested: shellWindow && shellWindow.showBeta

            Text {
                anchors.centerIn: parent
                text: "beta"
            }
        }
    }

    Component {
        id: gammaCompact

        Item {
            Text {
                anchors.centerIn: parent
                text: "gamma"
            }
        }
    }

    IslandHost {
        id: host

        anchors.fill: parent
        shellWindow: root
    }
}
)QML";

        std::unique_ptr<QObject> object(createFromData(qml));
        if (!object)
            return false;

        QVariant betaVisible;
        QVariant betaReason;
        if (!QMetaObject::invokeMethod(object.get(), "betaVisible", Q_RETURN_ARG(QVariant, betaVisible)))
            return false;
        if (!QMetaObject::invokeMethod(object.get(), "betaReason", Q_RETURN_ARG(QVariant, betaReason)))
            return false;
        if (betaVisible.toBool() || betaReason.toString() != QStringLiteral("plugin-hidden")) {
            qWarning() << "Expected beta to start plugin-hidden, got"
                       << betaVisible.toBool() << betaReason.toString();
            return false;
        }

        if (!QMetaObject::invokeMethod(object.get(), "enableBeta"))
            return false;
        QCoreApplication::processEvents();

        QVariant alphaVisible;
        QVariant betaVisibleAfter;
        QVariant gammaVisible;
        QVariant gammaReason;
        if (!QMetaObject::invokeMethod(object.get(), "alphaVisible", Q_RETURN_ARG(QVariant, alphaVisible)))
            return false;
        if (!QMetaObject::invokeMethod(object.get(), "betaVisible", Q_RETURN_ARG(QVariant, betaVisibleAfter)))
            return false;
        if (!QMetaObject::invokeMethod(object.get(), "gammaVisible", Q_RETURN_ARG(QVariant, gammaVisible)))
            return false;
        if (!QMetaObject::invokeMethod(object.get(), "gammaReason", Q_RETURN_ARG(QVariant, gammaReason)))
            return false;
        if (!alphaVisible.toBool() || !betaVisibleAfter.toBool()
            || gammaVisible.toBool()
            || gammaReason.toString() != QStringLiteral("space-pressure")) {
            qWarning() << "Expected alpha/beta visible and gamma hidden for space pressure, got"
                       << alphaVisible.toBool() << betaVisibleAfter.toBool()
                       << gammaVisible.toBool() << gammaReason.toString();
            return false;
        }
        return true;
    }

    bool islandModuleForwardsPointerHandlersToCompactView()
    {
        const QByteArray qml = R"QML(
import QtQuick
import ArchipelagoCore
import "file:///)QML" ARCHIPELAGO_SOURCE_DIR R"QML(/qml/components"

Item {
    id: root

    width: 220
    height: 64
    property var moduleRegistry: ({
        "swipe": {
            "compact": swipeCompact,
            "expanded": null,
            "anchors": ["left"],
            "defaultPriority": 80,
            "compactLayout": {
                "preferredWidth": 120,
                "minimumWidth": 80,
                "maximumWidth": 140
            }
        }
    })
    property var eventLog: []
    property bool activated: false

    function moduleEntry(moduleId) {
        return moduleRegistry && moduleId ? (moduleRegistry[moduleId] || {}) : {};
    }

    function compactComponentFor(moduleId) {
        return moduleEntry(moduleId).compact || null;
    }

    function expandedComponentFor(moduleId) {
        return moduleEntry(moduleId).expanded || null;
    }

    function record(eventName) {
        eventLog = eventLog.concat([eventName]);
    }

    function eventLogText() {
        return eventLog.join(",");
    }

    function resetState() {
        eventLog = [];
        activated = false;
    }

    function capsuleItem() {
        const children = module.children || [];
        for (let index = 0; index < children.length; ++index) {
            const child = children[index];
            if (child && child.pointerPressed !== undefined && child.primaryClicked !== undefined)
                return child;
        }
        return null;
    }

    function simulateSwipe() {
        resetState();
        const capsule = capsuleItem();
        if (!capsule)
            return false;
        capsule.pointerPressed(10, 12, Qt.LeftButton, Qt.LeftButton);
        capsule.pointerMoved(72, 12, Qt.LeftButton);
        capsule.pointerReleased(72, 12, Qt.LeftButton, 0);
        capsule.primaryClicked();
        return true;
    }

    function simulateTap() {
        resetState();
        const capsule = capsuleItem();
        if (!capsule)
            return false;
        capsule.pointerPressed(10, 12, Qt.LeftButton, Qt.LeftButton);
        capsule.pointerReleased(12, 12, Qt.LeftButton, 0);
        capsule.primaryClicked();
        return true;
    }

    Component {
        id: swipeCompact

        Item {
            property real startX: 0
            property bool suppressClick: false
            property var handlers: ({
                "pointerPressed": function(x) {
                    startX = x;
                    suppressClick = false;
                    root.record("press");
                },
                "pointerMoved": function(x) {
                    if (Math.abs(x - startX) > 24)
                        suppressClick = true;
                    root.record("move");
                },
                "pointerReleased": function() {
                    root.record(suppressClick ? "release-swipe" : "release-tap");
                },
                "primaryClicked": function() {
                    root.record(suppressClick ? "suppress-click" : "allow-click");
                    const handled = suppressClick;
                    suppressClick = false;
                    return handled;
                }
            })

            Text {
                anchors.centerIn: parent
                text: "swipe"
            }
        }
    }

    Item {
        id: host

        property var shellWindow: root

        function compactFor(moduleId) {
            return parent && parent.compactComponentFor ? parent.compactComponentFor(moduleId) : null;
        }

        function expandedComponentFor(moduleId) {
            return parent && parent.expandedComponentFor ? parent.expandedComponentFor(moduleId) : null;
        }

        function activateModule(moduleId, item) {
            root.activated = true;
        }
    }

    IslandModule {
        id: module

        moduleId: "swipe"
        host: host
        compactLevel: 0
    }
}
)QML";

        std::unique_ptr<QObject> object(createFromData(qml));
        if (!object)
            return false;

        if (!QMetaObject::invokeMethod(object.get(), "simulateSwipe"))
            return false;

        QVariant swipeLog;
        QVariant swipeActivated;
        if (!QMetaObject::invokeMethod(object.get(), "eventLogText", Q_RETURN_ARG(QVariant, swipeLog)))
            return false;
        swipeActivated = object->property("activated");
        if (swipeLog.toString() != QStringLiteral("press,move,release-swipe,suppress-click")
            || swipeActivated.toBool()) {
            qWarning() << "Expected swipe to suppress click and avoid activation, got"
                       << swipeLog.toString() << swipeActivated.toBool();
            return false;
        }

        if (!QMetaObject::invokeMethod(object.get(), "simulateTap"))
            return false;

        QVariant tapLog;
        QVariant tapActivated;
        if (!QMetaObject::invokeMethod(object.get(), "eventLogText", Q_RETURN_ARG(QVariant, tapLog)))
            return false;
        tapActivated = object->property("activated");
        if (tapLog.toString() != QStringLiteral("press,release-tap,allow-click")
            || !tapActivated.toBool()) {
            qWarning() << "Expected tap to allow activation, got"
                       << tapLog.toString() << tapActivated.toBool();
            return false;
        }
        return true;
    }
};

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QTemporaryDir configHome;
    if (configHome.isValid())
        qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());
    QGuiApplication app(argc, argv);
    ArchipelagoQmlTests tests;
    tests.initTestCase();

    if (!tests.timePluginComponentsLoad())
        return 1;
    if (!tests.compactViewUpdatesWhenRegistryArrives())
        return 1;
    if (!tests.islandHostUsesShellWindowForRegistry())
        return 1;
    if (!tests.dynamicLayoutHonorsSoftConfiguredWidthsAndRuntimePreferredWidths())
        return 1;
    if (!tests.dynamicLayoutHonorsVisibilityRequestsAndPriorityHiding())
        return 1;
    if (!tests.islandModuleForwardsPointerHandlersToCompactView())
        return 1;
    return 0;
}
