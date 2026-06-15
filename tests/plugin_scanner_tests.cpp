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
    QString m_basePath;

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
        m_basePath = m_tempDir.path();
        qputenv("ARCHIPELAGO_PLUGINS_DIR", m_basePath.toLocal8Bit());
    }

    void init()
    {
        // Wipe any plugin dirs written by previous tests so each
        // test starts from a known-empty pluginsBase.
        QDir(m_basePath).removeRecursively();
        QDir().mkpath(m_basePath);
    }

    void emptyPluginsDirYieldsEmptyList()
    {
        PluginScanner scanner;
        QCOMPARE(scanner.plugins().size(), 0);
        QCOMPARE(scanner.pluginsBase(), m_basePath);
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
                           "dataNeeds": ["niriService"]
                       })",
                       true, true);

        PluginScanner scanner;
        const QVariantList list = scanner.plugins();
        QCOMPARE(list.size(), 1);

        const QVariantMap entry = list.first().toMap();
        QCOMPARE(entry.value("id").toString(), QStringLiteral("workspaces"));
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
        QCOMPARE(entry.value("source").toString(), QStringLiteral("manifest"));
    }

    void manifestUsesDirectoryNameAsFallbackId()
    {
        writePluginDir("clock", "", true, true);
        PluginScanner scanner;
        const QVariantList list = scanner.plugins();
        QCOMPARE(list.size(), 1);
        const QVariantMap entry = list.first().toMap();
        QCOMPARE(entry.value("id").toString(), QStringLiteral("clock"));
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
