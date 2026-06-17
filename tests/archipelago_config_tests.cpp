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
    void acceptsUnknownPluginModules();
    void unknownPluginModuleWithoutExplicitWidthStaysDynamic();
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
    QCOMPARE(config.anchorModules(QStringLiteral("left")), QVariantList());
    QCOMPARE(config.anchorModules(QStringLiteral("center")), QVariantList({QStringLiteral("time")}));
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList());
    QCOMPARE(config.anchorModules(QStringLiteral("overlay")), QVariantList());
    QCOMPARE(config.islandHeight(), 38);
    QCOMPARE(config.islandGap(), 10);
    QCOMPARE(config.textFontFamily(), QStringLiteral("Inter Display"));
    QCOMPARE(config.iconFontFamily(), QStringLiteral("JetBrainsMono Nerd Font"));

    const QVariantMap time = moduleConfig(config, QStringLiteral("time"));
    QCOMPARE(time.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(time.value(QStringLiteral("priority")).toInt(), 50);
    QCOMPARE(time.value(QStringLiteral("compact")).toString(), QStringLiteral("time"));
    QCOMPARE(time.value(QStringLiteral("width")).toInt(), 108);
}

void ArchipelagoConfigTests::loadsAnchorsModulesFontsAndSizes()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        "anchors": {
            "left": ["time", "community.workspaces"],
            "center": [],
            "right": ["community.system"],
            "overlay": ["community.notifications"]
        },
        "modules": {
            "time": {
                "enabled": true,
                "priority": 75,
                "compact": "date",
                "actions": { "primary": "toggle", "secondary": "calendar" }
            },
            "community.media": { "enabled": false }
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
    QCOMPARE(config.anchorModules(QStringLiteral("left")), QVariantList({QStringLiteral("time"), QStringLiteral("community.workspaces")}));
    QCOMPARE(config.anchorModules(QStringLiteral("center")), QVariantList());
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("community.system")}));
    QCOMPARE(config.anchorModules(QStringLiteral("overlay")), QVariantList({QStringLiteral("community.notifications")}));
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

    const QVariantMap time = moduleConfig(config, QStringLiteral("time"));
    QCOMPARE(time.value(QStringLiteral("priority")).toInt(), 75);
    QCOMPARE(time.value(QStringLiteral("compact")).toString(), QStringLiteral("date"));
    QCOMPARE(config.moduleAction(QStringLiteral("time"), QStringLiteral("secondary")), QStringLiteral("calendar"));

    const QVariantMap media = moduleConfig(config, QStringLiteral("community.media"));
    QCOMPARE(media.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(media.value(QStringLiteral("priority")).toInt(), 0);
    QVERIFY(!media.contains(QStringLiteral("width")));
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
    QCOMPARE(config.anchorModules(QStringLiteral("center")), QVariantList({QStringLiteral("time")}));
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList());
    QCOMPARE(config.islandHeight(), 38);
    QCOMPARE(config.textFontFamily(), QStringLiteral("Inter Display"));
}

void ArchipelagoConfigTests::validatesRangesAndTypes()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        "anchors": { "left": "community.workspaces", "right": ["community.system", 7, "time"] },
        "modules": {
            "community.system": { "enabled": "yes", "priority": 140, "compact": 42 },
            "time": { "priority": -4 }
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
    QCOMPARE(config.anchorModules(QStringLiteral("left")), QVariantList());
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("community.system"), QStringLiteral("time")}));
    QCOMPARE(config.textFontFamily(), QStringLiteral("Inter Display"));
    QCOMPARE(config.bodyFontSize(), 16);
    QCOMPARE(config.islandHeight(), 38);
    QCOMPARE(config.islandGap(), 10);
    QCOMPARE(config.expandedWidth(), 520);
    QCOMPARE(config.morphDuration(), 480);

    const QVariantMap system = moduleConfig(config, QStringLiteral("community.system"));
    QCOMPARE(system.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(system.value(QStringLiteral("priority")).toInt(), 0);
    QCOMPARE(system.value(QStringLiteral("compact")).toString(), QString());
}

void ArchipelagoConfigTests::acceptsUnknownPluginModules()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        "anchors": {
            "right": ["community.weather"]
        },
        "modules": {
            "community.weather": {
                "enabled": false,
                "priority": 42,
                "width": 180,
                "actions": { "primary": "toggle" }
            }
        }
    })json";
    QVERIFY(!writeConfig(configHome, json).isEmpty());

    ArchipelagoConfig config;
    QCOMPARE(config.configError(), QString());
    QCOMPARE(config.anchorModules(QStringLiteral("right")), QVariantList({QStringLiteral("community.weather")}));

    const QVariantMap module = moduleConfig(config, QStringLiteral("community.weather"));
    QCOMPARE(module.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(module.value(QStringLiteral("priority")).toInt(), 42);
    QCOMPARE(module.value(QStringLiteral("width")).toInt(), 180);
    QCOMPARE(config.moduleAction(QStringLiteral("community.weather"), QStringLiteral("primary")), QStringLiteral("toggle"));
}

void ArchipelagoConfigTests::unknownPluginModuleWithoutExplicitWidthStaysDynamic()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());

    const QByteArray json = R"json({
        "anchors": {
            "right": ["media"]
        },
        "modules": {
            "media": {
                "enabled": true,
                "priority": 60
            }
        }
    })json";
    QVERIFY(!writeConfig(configHome, json).isEmpty());

    ArchipelagoConfig config;
    QCOMPARE(config.configError(), QString());

    const QVariantMap module = moduleConfig(config, QStringLiteral("media"));
    QCOMPARE(module.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(module.value(QStringLiteral("priority")).toInt(), 60);
    QVERIFY(!module.contains(QStringLiteral("width")));
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
