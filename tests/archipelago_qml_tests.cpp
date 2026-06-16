#include <memory>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
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
};

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ArchipelagoQmlTests tests;
    tests.initTestCase();

    if (!tests.timePluginComponentsLoad())
        return 1;
    if (!tests.compactViewUpdatesWhenRegistryArrives())
        return 1;
    if (!tests.islandHostUsesShellWindowForRegistry())
        return 1;
    return 0;
}
