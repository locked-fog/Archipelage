#include "NiriCompositorService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class NiriCompositorServiceTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesWorkspaceSnapshot();
    void parsesOutputSnapshot();
    void parsesWindowSnapshot();
    void parsesFloatingWindowGeometry();
    void appliesWorkspaceAndWindowEvents();
    void classifiesOkAndErrReplies();
};

void NiriCompositorServiceTests::parsesWorkspaceSnapshot()
{
    const QByteArray raw = R"json([
        {"id": 1, "idx": 1, "name": null, "output": "DP-1",
         "is_active": true, "is_focused": true, "active_window_id": 42},
        {"id": 2, "idx": 2, "name": "web", "output": null,
         "is_active": false, "is_focused": false, "active_window_id": null}
    ])json";
    const QJsonArray array = QJsonDocument::fromJson(raw).array();
    const QVariantList parsed = NiriCompositorService::parseWorkspaces(array);
    QCOMPARE(parsed.size(), 2);

    const QVariantMap first = parsed.at(0).toMap();
    QCOMPARE(first.value(QStringLiteral("idx")).toInt(), 1);
    QCOMPARE(first.value(QStringLiteral("name")).toString(), QString());
    QCOMPARE(first.value(QStringLiteral("displayName")).toString(), QStringLiteral("1"));
    QCOMPARE(first.value(QStringLiteral("output")).toString(), QStringLiteral("DP-1"));
    QCOMPARE(first.value(QStringLiteral("isFocused")).toBool(), true);
    QCOMPARE(first.value(QStringLiteral("activeWindowId")).toULongLong(), qulonglong(42));

    const QVariantMap second = parsed.at(1).toMap();
    QCOMPARE(second.value(QStringLiteral("name")).toString(), QStringLiteral("web"));
    QCOMPARE(second.value(QStringLiteral("displayName")).toString(), QStringLiteral("web"));
    QCOMPARE(second.value(QStringLiteral("output")).toString(), QString());
}

void NiriCompositorServiceTests::parsesOutputSnapshot()
{
    const QByteArray raw = R"json([
        {"name": "DP-1", "logical": {"x": 0, "y": 0, "width": 1920, "height": 1080},
         "scale": 1.0, "transform": "Normal"},
        {"name": "HDMI-A-1", "logical": {"x": 1920, "y": 0, "width": 2560, "height": 1440},
         "scale": 1.5, "transform": "Normal"}
    ])json";
    const QJsonArray array = QJsonDocument::fromJson(raw).array();
    const QVariantList parsed = NiriCompositorService::parseOutputs(array);
    QCOMPARE(parsed.size(), 2);

    const QVariantMap first = parsed.at(0).toMap();
    QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("DP-1"));
    QCOMPARE(first.value(QStringLiteral("width")).toInt(), 1920);
    QCOMPARE(first.value(QStringLiteral("scale")).toDouble(), 1.0);

    const QVariantMap second = parsed.at(1).toMap();
    QCOMPARE(second.value(QStringLiteral("x")).toInt(), 1920);
    QCOMPARE(second.value(QStringLiteral("height")).toInt(), 1440);
    QCOMPARE(second.value(QStringLiteral("scale")).toDouble(), 1.5);
}

void NiriCompositorServiceTests::parsesWindowSnapshot()
{
    const QByteArray raw = R"json([
        {"id": 7, "title": "Editor", "app_id": "code", "workspace_id": 1,
         "is_focused": true, "is_floating": false,
         "layout": {"Tiled": {"x": 100, "y": 50, "width": 800, "height": 600}}}
    ])json";
    const QJsonArray array = QJsonDocument::fromJson(raw).array();
    const QVariantList parsed = NiriCompositorService::parseWindows(array);
    QCOMPARE(parsed.size(), 1);

    const QVariantMap map = parsed.at(0).toMap();
    QCOMPARE(map.value(QStringLiteral("id")).toULongLong(), qulonglong(7));
    QCOMPARE(map.value(QStringLiteral("title")).toString(), QStringLiteral("Editor"));
    QCOMPARE(map.value(QStringLiteral("appId")).toString(), QStringLiteral("code"));
    QCOMPARE(map.value(QStringLiteral("workspaceId")).toInt(), 1);
    QCOMPARE(map.value(QStringLiteral("isFloating")).toBool(), false);
    QCOMPARE(map.value(QStringLiteral("isFocused")).toBool(), true);

    const QVariantList at = map.value(QStringLiteral("at")).toList();
    QCOMPARE(at.size(), 2);
    QCOMPARE(at.at(0).toInt(), 100);
    QCOMPARE(at.at(1).toInt(), 50);

    const QVariantList size = map.value(QStringLiteral("size")).toList();
    QCOMPARE(size.at(0).toInt(), 800);
    QCOMPARE(size.at(1).toInt(), 600);
}

void NiriCompositorServiceTests::parsesFloatingWindowGeometry()
{
    const QByteArray raw = R"json([
        {"id": 9, "title": "Sticky", "app_id": "kate", "workspace_id": null,
         "is_focused": false, "is_floating": true,
         "layout": {"Floating": {"x": 200, "y": 300, "width": 640, "height": 480}}}
    ])json";
    const QJsonArray array = QJsonDocument::fromJson(raw).array();
    const QVariantList parsed = NiriCompositorService::parseWindows(array);
    QCOMPARE(parsed.size(), 1);

    const QVariantMap map = parsed.at(0).toMap();
    QCOMPARE(map.value(QStringLiteral("isFloating")).toBool(), true);
    const QVariantList at = map.value(QStringLiteral("at")).toList();
    QCOMPARE(at.at(0).toInt(), 200);
    QCOMPARE(at.at(1).toInt(), 300);
    const QVariantList size = map.value(QStringLiteral("size")).toList();
    QCOMPARE(size.at(0).toInt(), 640);
    QCOMPARE(size.at(1).toInt(), 480);
}

void NiriCompositorServiceTests::appliesWorkspaceAndWindowEvents()
{
    qunsetenv("NIRI_SOCKET");
    NiriCompositorService service;
    QSignalSpy workspaceSpy(&service, &NiriCompositorService::workspacesChanged);
    QSignalSpy focusedWorkspaceSpy(&service, &NiriCompositorService::focusedWorkspaceChanged);
    QSignalSpy windowsSpy(&service, &NiriCompositorService::windowsChanged);
    QSignalSpy focusedWindowSpy(&service, &NiriCompositorService::focusedWindowChanged);

    const QJsonObject workspacesEvent = QJsonDocument::fromJson(R"json({
        "WorkspacesChanged": {
            "focused_workspace_index": 2,
            "workspaces": [
                {"id": 1, "idx": 1, "name": null, "output": "DP-1", "is_active": true, "is_focused": false, "active_window_id": null},
                {"id": 2, "idx": 2, "name": "web", "output": "DP-1", "is_active": true, "is_focused": true, "active_window_id": 9}
            ]
        }
    })json").object();
    QVERIFY(service.applyEventForTest(workspacesEvent));
    QCOMPARE(workspaceSpy.size(), 1);
    QCOMPARE(focusedWorkspaceSpy.size(), 1);
    QCOMPARE(service.focusedWorkspaceIdx(), 2);
    QCOMPARE(service.focusedWorkspace().value(QStringLiteral("displayName")).toString(), QStringLiteral("web"));

    const QJsonObject windowsEvent = QJsonDocument::fromJson(R"json({
        "WindowsChanged": {
            "windows": [
                {"id": 9, "title": "Browser", "app_id": "firefox", "workspace_id": 2, "is_focused": true, "is_floating": false}
            ]
        }
    })json").object();
    QVERIFY(service.applyEventForTest(windowsEvent));
    QCOMPARE(windowsSpy.size(), 1);
    QCOMPARE(focusedWindowSpy.size(), 1);
    QCOMPARE(service.focusedWindowId(), qulonglong(9));
    QCOMPARE(service.focusedWindow().value(QStringLiteral("appId")).toString(), QStringLiteral("firefox"));

    const QJsonObject closedEvent = QJsonDocument::fromJson(R"json({
        "WindowClosed": {"id": 9}
    })json").object();
    QVERIFY(service.applyEventForTest(closedEvent));
    QCOMPARE(service.windows().size(), 0);
    QCOMPARE(service.focusedWindowId(), qulonglong(0));
}

void NiriCompositorServiceTests::classifiesOkAndErrReplies()
{
    const QJsonObject ok = QJsonDocument::fromJson(R"json({"Ok": [{"id": 1, "idx": 1}]})json").object();
    const QString okText = NiriCompositorService::replySummary(ok, QStringLiteral("test"));
    QVERIFY(okText.startsWith(QStringLiteral("array[")));

    const QJsonObject err = QJsonDocument::fromJson(R"json({"Err": "no such workspace"})json").object();
    const QString errText = NiriCompositorService::replySummary(err, QStringLiteral("test"));
    QCOMPARE(errText, QStringLiteral("error:no such workspace"));

    const QJsonObject nullOk = QJsonDocument::fromJson(R"json({"Ok": null})json").object();
    QCOMPARE(NiriCompositorService::replySummary(nullOk, QString()), QStringLiteral("ok"));
}

QTEST_MAIN(NiriCompositorServiceTests)
#include "niri_compositor_service_tests.moc"
