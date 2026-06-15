#include "ArchipelagoConfig.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class ArchipelagoConfigTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesArchipelagoConfigPathAndDefaults();
    void loadsAnchorsModulesFontsAndSizes();
    void stripsComments();
    void reportsInvalidJsonAndKeepsDefaults();
    void validatesRangesAndTypes();
    void mapsConfiguredMouseButtons();
};

namespace {
QString writeConfig(QTemporaryDir &configHome, const QByteArray &json)
{
    const QString configDir = configHome.path() + QStringLiteral("/archipelago");
    QDir().mkpath(configDir);

    const QString configPath = configDir + QStringLiteral("/config.json");
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();

    file.write(json);
    return configPath;
}

QVariantMap moduleConfig(const ArchipelagoConfig &config, const QString &moduleId)
{
    return config.moduleConfig(moduleId).toMap();
}
}  // namespace

void ArchipelagoConfigTests::exposesArchipelagoConfigPathAndDefaults()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    ArchipelagoConfig config;

    QCOMPARE(config.userConfigPath(), configHome.path() + QStringLiteral("/archipelago/config.json"));
    QCOMPARE(config.configError(), QString());
    QCOMPARE(config.anchorModules(QStringLiteral("left")), QVariantList({QStringLiteral("workspaces")}));
    QCOMPARE(config.anchorModules(QStringLiteral("center")), QVariantList({QStringLiteral("clock")}));
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("media"), QStringLiteral("system")}));
    QCOMPARE(config.anchorModules(QStringLiteral("overlay")), QVariantList({QStringLiteral("notifications")}));
    QCOMPARE(config.islandHeight(), 38);
    QCOMPARE(config.islandGap(), 10);
    QCOMPARE(config.textFontFamily(), QStringLiteral("Inter Display"));
    QCOMPARE(config.iconFontFamily(), QStringLiteral("JetBrainsMono Nerd Font"));

    const QVariantMap workspace = moduleConfig(config, QStringLiteral("workspaces"));
    QCOMPARE(workspace.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(workspace.value(QStringLiteral("priority")).toInt(), 90);
    QCOMPARE(workspace.value(QStringLiteral("compact")).toString(), QStringLiteral("focused"));
}

void ArchipelagoConfigTests::loadsAnchorsModulesFontsAndSizes()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        "anchors": {
            "left": ["clock", "workspaces"],
            "center": [],
            "right": ["system"],
            "overlay": ["notifications"]
        },
        "modules": {
            "clock": {
                "enabled": true,
                "priority": 75,
                "compact": "date",
                "actions": { "primary": "toggle", "secondary": "calendar" }
            },
            "media": { "enabled": false }
        },
        "fonts": {
            "text": "Test Sans",
            "icon": "Test Nerd",
            "time": "Test Mono",
            "bodySize": 15,
            "titleSize": 19,
            "iconSize": 17
        },
        "sizes": {
            "islandHeight": 42,
            "islandGap": 14,
            "edgeMargin": 18,
            "expandedVerticalOffset": 16,
            "expandedWidth": 620,
            "expandedHeight": 430
        },
        "animations": {
            "morphMs": 360,
            "compactFadeMs": 140,
            "contentFadeMs": 180
        },
        "style": {
            "panel": "#010203",
            "accent": "#123456"
        },
        "compositor": "niri"
    })json";
    QVERIFY(!writeConfig(configHome, json).isEmpty());

    ArchipelagoConfig config;

    QCOMPARE(config.configError(), QString());
    QCOMPARE(config.anchorModules(QStringLiteral("left")), QVariantList({QStringLiteral("clock"), QStringLiteral("workspaces")}));
    QCOMPARE(config.anchorModules(QStringLiteral("center")), QVariantList());
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("system")}));
    QCOMPARE(config.textFontFamily(), QStringLiteral("Test Sans"));
    QCOMPARE(config.iconFontFamily(), QStringLiteral("Test Nerd"));
    QCOMPARE(config.timeFontFamily(), QStringLiteral("Test Mono"));
    QCOMPARE(config.bodyFontSize(), 15);
    QCOMPARE(config.titleFontSize(), 19);
    QCOMPARE(config.iconFontSize(), 17);
    QCOMPARE(config.islandHeight(), 42);
    QCOMPARE(config.islandGap(), 14);
    QCOMPARE(config.edgeMargin(), 18);
    QCOMPARE(config.expandedVerticalOffset(), 16);
    QCOMPARE(config.expandedWidth(), 620);
    QCOMPARE(config.expandedHeight(), 430);
    QCOMPARE(config.morphDuration(), 360);
    QCOMPARE(config.compactFadeDuration(), 140);
    QCOMPARE(config.contentFadeDuration(), 180);
    QCOMPARE(config.styleOverride(QStringLiteral("panel")), QStringLiteral("#010203"));
    QCOMPARE(config.compositor(), QStringLiteral("niri"));

    const QVariantMap clock = moduleConfig(config, QStringLiteral("clock"));
    QCOMPARE(clock.value(QStringLiteral("priority")).toInt(), 75);
    QCOMPARE(clock.value(QStringLiteral("compact")).toString(), QStringLiteral("date"));
    QCOMPARE(config.moduleAction(QStringLiteral("clock"), QStringLiteral("secondary")), QStringLiteral("calendar"));

    const QVariantMap media = moduleConfig(config, QStringLiteral("media"));
    QCOMPARE(media.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(media.value(QStringLiteral("priority")).toInt(), 60);
}

void ArchipelagoConfigTests::stripsComments()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        // Keep this comment outside strings.
        "fonts": {
            "text": "Slash // preserved"
        },
        /*
          Block comment
        */
        "sizes": { "islandHeight": 44 }
    })json";
    QVERIFY(!writeConfig(configHome, json).isEmpty());

    ArchipelagoConfig config;
    QCOMPARE(config.configError(), QString());
    QCOMPARE(config.textFontFamily(), QStringLiteral("Slash // preserved"));
    QCOMPARE(config.islandHeight(), 44);
}

void ArchipelagoConfigTests::reportsInvalidJsonAndKeepsDefaults()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    QVERIFY(!writeConfig(configHome, "{").isEmpty());

    ArchipelagoConfig config;
    QVERIFY(!config.configError().isEmpty());
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("media"), QStringLiteral("system")}));
    QCOMPARE(config.islandHeight(), 38);
    QCOMPARE(config.textFontFamily(), QStringLiteral("Inter Display"));
}

void ArchipelagoConfigTests::validatesRangesAndTypes()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        "anchors": { "left": "workspaces", "right": ["system", 7, "clock"] },
        "modules": {
            "system": { "enabled": "yes", "priority": 140, "compact": 42 },
            "clock": { "priority": -4 }
        },
        "fonts": {
            "text": "",
            "bodySize": 5
        },
        "sizes": {
            "islandHeight": 8,
            "islandGap": 1000,
            "expandedWidth": 42
        },
        "animations": {
            "morphMs": 4
        }
    })json";
    QVERIFY(!writeConfig(configHome, json).isEmpty());

    ArchipelagoConfig config;
    QVERIFY(config.configError().contains(QStringLiteral("Invalid")));
    QCOMPARE(config.anchorModules(QStringLiteral("left")), QVariantList({QStringLiteral("workspaces")}));
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("system"), QStringLiteral("clock")}));
    QCOMPARE(config.textFontFamily(), QStringLiteral("Inter Display"));
    QCOMPARE(config.bodyFontSize(), 16);
    QCOMPARE(config.islandHeight(), 38);
    QCOMPARE(config.islandGap(), 10);
    QCOMPARE(config.expandedWidth(), 520);
    QCOMPARE(config.morphDuration(), 380);

    const QVariantMap system = moduleConfig(config, QStringLiteral("system"));
    QCOMPARE(system.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(system.value(QStringLiteral("priority")).toInt(), 80);
    QCOMPARE(system.value(QStringLiteral("compact")).toString(), QStringLiteral("cluster"));
}

void ArchipelagoConfigTests::mapsConfiguredMouseButtons()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    ArchipelagoConfig config;
    QCOMPARE(config.mouseButton(1), int(Qt::LeftButton));
    QCOMPARE(config.mouseButton(2), int(Qt::MiddleButton));
    QCOMPARE(config.mouseButton(3), int(Qt::RightButton));
    QCOMPARE(config.mouseButton(8), 8);
    QCOMPARE(config.mouseButton(QVariant()), int(Qt::NoButton));
    QCOMPARE(config.mouseButtonsMask(QVariantList({1, 3})), int(Qt::LeftButton | Qt::RightButton));
}

QTEST_MAIN(ArchipelagoConfigTests)

#include "archipelago_config_tests.moc"
