#include "PluginScanner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest/QtTest>

class PluginScannerTests : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;
    QString m_envPluginsPath;
    QString m_userDataHome;
    QString m_userPluginsPath;
    QString m_systemDataDir;
    QString m_systemPluginsPath;
    QString m_basePath;

    void resetDir(const QString &path)
    {
        QDir(path).removeRecursively();
        QDir().mkpath(path);
    }

    void useEnvOverride()
    {
        m_basePath = m_envPluginsPath;
        qputenv("ARCHIPELAGO_PLUGINS_DIR", m_envPluginsPath.toLocal8Bit());
    }

    void useDefaultSearchPaths()
    {
        qunsetenv("ARCHIPELAGO_PLUGINS_DIR");
    }

    QString writePluginDir(const QString &name, const QString &manifestJson, bool writeCompact, bool writeExpanded)
    {
        const QString path = m_basePath + QLatin1Char('/') + name;
        QDir().mkpath(path);
        if (!manifestJson.isEmpty()) {
            QFile f(path + QStringLiteral("/manifest.json"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream(&f) << manifestJson;
                f.close();
            }
        }
        if (writeCompact) {
            QFile f(path + QStringLiteral("/Compact.qml"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream(&f) << "import QtQuick\nItem {}\n";
                f.close();
            }
        }
        if (writeExpanded) {
            QFile f(path + QStringLiteral("/Expanded.qml"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream(&f) << "import QtQuick\nItem {}\n";
                f.close();
            }
        }
        return path;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        m_envPluginsPath = m_tempDir.path() + QStringLiteral("/env_plugins");
        m_userDataHome = m_tempDir.path() + QStringLiteral("/xdg_home");
        m_userPluginsPath = m_userDataHome + QStringLiteral("/archipelago/plugins");
        m_systemDataDir = m_tempDir.path() + QStringLiteral("/xdg_system");
        m_systemPluginsPath = m_systemDataDir + QStringLiteral("/archipelago/plugins");

        const QString isolatedCwd = m_tempDir.path() + QStringLiteral("/cwd");
        QVERIFY(QDir().mkpath(isolatedCwd));
        QVERIFY(QDir::setCurrent(isolatedCwd));
    }

    void init()
    {
        // Wipe any plugin dirs written by previous tests so each
        // test starts from known-empty plugin roots.
        resetDir(m_envPluginsPath);
        QDir(m_userDataHome).removeRecursively();
        QDir().mkpath(m_userDataHome);
        QDir(m_systemDataDir).removeRecursively();
        QDir().mkpath(m_systemDataDir);

        qputenv("XDG_DATA_HOME", m_userDataHome.toLocal8Bit());
        qputenv("XDG_DATA_DIRS", m_systemDataDir.toLocal8Bit());
        useEnvOverride();
    }

    void emptyPluginsDirYieldsEmptyList()
    {
        PluginScanner scanner;
        QCOMPARE(scanner.plugins().size(), 0);
        QCOMPARE(scanner.pluginsBase(), m_envPluginsPath);
    }

    void userPluginDirectoryIsDiscovered()
    {
        useDefaultSearchPaths();
        m_basePath = m_userPluginsPath;
        writePluginDir("third-party", R"({"id": "third-party", "label": "Third Party"})", true, true);

        PluginScanner scanner;
        QCOMPARE(scanner.pluginsBase(), m_userPluginsPath);
        const QVariantMap entry = scanner.manifestFor("third-party");
        QCOMPARE(entry.value("label").toString(), QStringLiteral("Third Party"));
        QCOMPARE(entry.value("directoryPath").toString(), m_userPluginsPath + QStringLiteral("/third-party"));
    }

    void systemPluginDirectoryIsDiscovered()
    {
        useDefaultSearchPaths();
        m_basePath = m_systemPluginsPath;
        writePluginDir("system-third-party", R"({"id": "system-third-party"})", true, false);

        PluginScanner scanner;
        QCOMPARE(scanner.pluginsBase(), m_systemPluginsPath);
        QVERIFY(!scanner.manifestFor("system-third-party").isEmpty());
    }

    void envPluginDirectoryOverridesDefaultRoots()
    {
        writePluginDir("env-only", R"({"id": "env-only"})", true, false);

        useDefaultSearchPaths();
        m_basePath = m_userPluginsPath;
        writePluginDir("user-only", R"({"id": "user-only"})", true, false);

        useEnvOverride();
        PluginScanner scanner;
        QVERIFY(!scanner.manifestFor("env-only").isEmpty());
        QVERIFY(scanner.manifestFor("user-only").isEmpty());
        QCOMPARE(scanner.plugins().size(), 1);
    }

    void manifestParsedWithAllFields()
    {
        writePluginDir("workspaces",
                       R"({
                           "id": "workspaces",
                           "label": "Workspaces",
                           "version": "1.0.0",
                           "anchors": ["left", "center"],
                           "defaultPriority": 90,
                           "compact": "Compact.qml",
                           "expanded": "Expanded.qml",
                           "preferredWidth": 680,
                           "preferredHeight": 430,
                           "dataNeeds": ["niriService"],
                           "previewTemplates": [
                               {
                                   "id": "message",
                                   "component": "MessagePreview.qml",
                                   "defaultWidth": 380,
                                   "defaultHeight": 104,
                                   "timeoutMs": 5000,
                                   "maxVisible": 5,
                                   "focusPolicy": "passive"
                               }
                           ]
                       })",
                       true, true);

        PluginScanner scanner;
        const QVariantList list = scanner.plugins();
        QCOMPARE(list.size(), 1);

        const QVariantMap entry = list.first().toMap();
        QCOMPARE(entry.value("id").toString(), QStringLiteral("workspaces"));
        QCOMPARE(entry.value("directoryName").toString(), QStringLiteral("workspaces"));
        QCOMPARE(entry.value("directoryPath").toString(), m_envPluginsPath + QStringLiteral("/workspaces"));
        QCOMPARE(entry.value("label").toString(), QStringLiteral("Workspaces"));
        QCOMPARE(entry.value("version").toString(), QStringLiteral("1.0.0"));
        QCOMPARE(entry.value("anchors").toStringList(),
                 QStringList({QStringLiteral("left"), QStringLiteral("center")}));
        QCOMPARE(entry.value("defaultPriority").toInt(), 90);
        QCOMPARE(entry.value("compact").toString(), QStringLiteral("Compact.qml"));
        QCOMPARE(entry.value("expanded").toString(), QStringLiteral("Expanded.qml"));
        QCOMPARE(entry.value("preferredWidth").toInt(), 680);
        QCOMPARE(entry.value("preferredHeight").toInt(), 430);
        QCOMPARE(entry.value("dataNeeds").toStringList(),
                 QStringList({QStringLiteral("niriService")}));
        const QVariantList previewTemplates = entry.value("previewTemplates").toList();
        QCOMPARE(previewTemplates.size(), 1);
        const QVariantMap preview = previewTemplates.first().toMap();
        QCOMPARE(preview.value("id").toString(), QStringLiteral("message"));
        QCOMPARE(preview.value("component").toString(), QStringLiteral("MessagePreview.qml"));
        QCOMPARE(preview.value("defaultWidth").toInt(), 380);
        QCOMPARE(preview.value("defaultHeight").toInt(), 104);
        QCOMPARE(preview.value("timeoutMs").toInt(), 5000);
        QCOMPARE(preview.value("maxVisible").toInt(), 5);
        QCOMPARE(preview.value("focusPolicy").toString(), QStringLiteral("passive"));
        QCOMPARE(entry.value("source").toString(), QStringLiteral("manifest"));
    }

    void manifestUsesDirectoryNameAsFallbackId()
    {
        writePluginDir("time", "", true, true);
        PluginScanner scanner;
        const QVariantList list = scanner.plugins();
        QCOMPARE(list.size(), 1);
        const QVariantMap entry = list.first().toMap();
        QCOMPARE(entry.value("id").toString(), QStringLiteral("time"));
        QCOMPARE(entry.value("directoryName").toString(), QStringLiteral("time"));
        QCOMPARE(entry.value("compact").toString(), QStringLiteral("Compact.qml"));
        QCOMPARE(entry.value("expanded").toString(), QStringLiteral("Expanded.qml"));
        QCOMPARE(entry.value("source").toString(), QStringLiteral("fallback"));
    }

    void invalidManifestJsonFallsBack()
    {
        writePluginDir("broken", "{not valid json", true, true);
        PluginScanner scanner;
        const QVariantList list = scanner.plugins();
        QCOMPARE(list.size(), 1);
        const QVariantMap entry = list.first().toMap();
        QCOMPARE(entry.value("id").toString(), QStringLiteral("broken"));
        QCOMPARE(entry.value("source").toString(), QStringLiteral("fallback"));
    }

    void manifestForReturnsEntryById()
    {
        writePluginDir("a", R"({"id": "a", "label": "Alpha"})", true, false);
        writePluginDir("b", R"({"id": "b", "label": "Beta"})", false, true);
        PluginScanner scanner;
        const QVariantMap entry = scanner.manifestFor("b");
        QCOMPARE(entry.value("label").toString(), QStringLiteral("Beta"));
    }

    void manifestForUnknownIdReturnsEmpty()
    {
        writePluginDir("a", R"({"id": "a"})", true, false);
        PluginScanner scanner;
        QVERIFY(scanner.manifestFor("does-not-exist").isEmpty());
    }

    void manifestIdMayDifferFromDirectoryName()
    {
        writePluginDir("community-time", R"({"id": "time.alt", "label": "Alt Time"})", true, true);
        PluginScanner scanner;
        const QVariantMap entry = scanner.manifestFor("time.alt");
        QCOMPARE(entry.value("id").toString(), QStringLiteral("time.alt"));
        QCOMPARE(entry.value("directoryName").toString(), QStringLiteral("community-time"));
    }

    void unsafeComponentPathsFallBackToConventions()
    {
        writePluginDir("unsafe",
                       R"({
                           "id": "unsafe",
                           "compact": "../Escape.qml",
                           "expanded": "Nested/View.qml",
                           "previewTemplates": [
                               { "id": "bad", "component": "../Bad.qml" },
                               { "id": "also-bad", "component": "Nested/View.qml" },
                               { "id": "good", "component": "GoodPreview.qml" }
                           ]
                       })",
                       true, true);
        PluginScanner scanner;
        const QVariantMap entry = scanner.manifestFor("unsafe");
        QCOMPARE(entry.value("compact").toString(), QStringLiteral("Compact.qml"));
        QCOMPARE(entry.value("expanded").toString(), QStringLiteral("Expanded.qml"));
        const QVariantList previewTemplates = entry.value("previewTemplates").toList();
        QCOMPARE(previewTemplates.size(), 1);
        QCOMPARE(previewTemplates.first().toMap().value("id").toString(), QStringLiteral("good"));
    }

    void unsafeDirectoryIdsAreIgnored()
    {
        writePluginDir("bad id", "", true, true);
        PluginScanner scanner;
        QCOMPARE(scanner.plugins().size(), 0);
    }

    void rescanPicksUpNewPlugin()
    {
        PluginScanner scanner;
        QCOMPARE(scanner.plugins().size(), 0);
        writePluginDir("latecomer", R"({"id": "latecomer"})", true, false);
        scanner.rescan();
        QCOMPARE(scanner.plugins().size(), 1);
    }
};

QTEST_GUILESS_MAIN(PluginScannerTests)
#include "plugin_scanner_tests.moc"
