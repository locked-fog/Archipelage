#include "ArchipelagoConfig.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>
#include <QStringList>
#include <Qt>

#include <cmath>
#include <utility>

namespace {
constexpr int kReloadDebounceMs = 50;

QByteArray stripJsonComments(const QByteArray &input)
{
    const QString text = QString::fromUtf8(input);
    QString output;
    output.reserve(text.size());

    bool inString = false;
    bool escaped = false;
    bool inLineComment = false;
    bool inBlockComment = false;

    for (int index = 0; index < text.size(); ++index) {
        const QChar ch = text.at(index);
        const QChar next = index + 1 < text.size() ? text.at(index + 1) : QChar();

        if (inLineComment) {
            if (ch == u'\n') {
                inLineComment = false;
                output.append(ch);
            }
            continue;
        }

        if (inBlockComment) {
            if (ch == u'*' && next == u'/') {
                inBlockComment = false;
                ++index;
            } else if (ch == u'\n') {
                output.append(ch);
            }
            continue;
        }

        if (inString) {
            output.append(ch);
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == u'\\') {
                escaped = true;
                continue;
            }
            if (ch == u'"')
                inString = false;
            continue;
        }

        if (ch == u'"') {
            inString = true;
            output.append(ch);
            continue;
        }

        if (ch == u'/' && next == u'/') {
            inLineComment = true;
            ++index;
            continue;
        }

        if (ch == u'/' && next == u'*') {
            inBlockComment = true;
            ++index;
            continue;
        }

        output.append(ch);
    }

    return output.toUtf8();
}

QVariantMap makeModule(bool enabled,
                       int priority,
                       const QString &compact,
                       QVariantMap actions,
                       QVariantMap scroll,
                       int width)
{
    QVariantMap module;
    module.insert(QStringLiteral("enabled"), enabled);
    module.insert(QStringLiteral("priority"), priority);
    module.insert(QStringLiteral("compact"), compact);
    module.insert(QStringLiteral("actions"), std::move(actions));
    module.insert(QStringLiteral("scroll"), std::move(scroll));
    module.insert(QStringLiteral("width"), width);
    return module;
}

QVariantMap defaultAnchors()
{
    QVariantMap anchors;
    anchors.insert(QStringLiteral("left"), QVariantList());
    anchors.insert(QStringLiteral("center"), QVariantList({QStringLiteral("time")}));
    anchors.insert(QStringLiteral("right"), QVariantList());
    anchors.insert(QStringLiteral("overlay"), QVariantList());
    return anchors;
}

QVariantMap defaultModules()
{
    QVariantMap modules;
    modules.insert(QStringLiteral("time"),
                   makeModule(true,
                              50,
                              QStringLiteral("time"),
                              {{QStringLiteral("primary"), QStringLiteral("toggle")},
                               {QStringLiteral("secondary"), QStringLiteral("calendar")}},
                              {},
                              108));
    return modules;
}

QVariantMap defaultModuleConfig()
{
    QVariantMap module;
    module.insert(QStringLiteral("enabled"), true);
    module.insert(QStringLiteral("priority"), 0);
    module.insert(QStringLiteral("compact"), QString());
    module.insert(QStringLiteral("actions"), QVariantMap());
    module.insert(QStringLiteral("scroll"), QVariantMap());
    return module;
}

bool isAllowedAnchor(const QString &anchor)
{
    static const QSet<QString> allowed = {
        QStringLiteral("left"),
        QStringLiteral("center"),
        QStringLiteral("right"),
        QStringLiteral("overlay"),
    };
    return allowed.contains(anchor);
}

bool isAllowedCompositor(const QString &value)
{
    static const QSet<QString> allowed = {
        QStringLiteral("auto"),
        QStringLiteral("niri"),
        QStringLiteral("hyprland"),
        QStringLiteral("none"),
    };
    return allowed.contains(value);
}

QString jsonString(const QJsonObject &object,
                   const QString &key,
                   const QString &fallback,
                   QStringList *errors,
                   const QString &path)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return fallback;
    if (value.isString() && !value.toString().trimmed().isEmpty())
        return value.toString();
    errors->append(QStringLiteral("Invalid %1; using default.").arg(path));
    return fallback;
}

bool jsonBool(const QJsonObject &object, const QString &key, bool fallback, QStringList *errors, const QString &path)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return fallback;
    if (value.isBool())
        return value.toBool();
    errors->append(QStringLiteral("Invalid %1; using default.").arg(path));
    return fallback;
}

int jsonIntRange(const QJsonObject &object,
                 const QString &key,
                 int fallback,
                 int minimum,
                 int maximum,
                 QStringList *errors,
                 const QString &path)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return fallback;
    if (!value.isDouble()) {
        errors->append(QStringLiteral("Invalid %1; using default.").arg(path));
        return fallback;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number)) {
        errors->append(QStringLiteral("Invalid %1; using default.").arg(path));
        return fallback;
    }

    const int rounded = qRound(number);
    if (rounded < minimum || rounded > maximum) {
        errors->append(QStringLiteral("Invalid %1; expected %2-%3.").arg(path).arg(minimum).arg(maximum));
        return fallback;
    }
    return rounded;
}

QVariantMap variantMapValue(const QVariantMap &map, const QString &key)
{
    const QVariant value = map.value(key);
    return value.canConvert<QVariantMap>() ? value.toMap() : QVariantMap();
}

template<typename T>
bool assignIfChanged(T &field, const T &next)
{
    if (field == next)
        return false;
    field = next;
    return true;
}
}  // namespace

ArchipelagoConfig::ArchipelagoConfig(QObject *parent)
    : QObject(parent)
    , m_userConfigPath(configHome() + QStringLiteral("/archipelago/config.json"))
    , m_anchors(defaultAnchors())
    , m_modules(defaultModules())
    , m_iconFontFamily(QStringLiteral("JetBrainsMono Nerd Font"))
    , m_textFontFamily(QStringLiteral("Inter Display"))
    , m_timeFontFamily(QStringLiteral("Inter Display"))
    , m_compositor(QStringLiteral("auto"))
{
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(kReloadDebounceMs);

    connect(&m_reloadTimer, &QTimer::timeout, this, &ArchipelagoConfig::loadConfig);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ArchipelagoConfig::scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ArchipelagoConfig::scheduleReload);

    loadConfig();
}

QString ArchipelagoConfig::userConfigPath() const { return m_userConfigPath; }
QString ArchipelagoConfig::configError() const { return m_configError; }
QVariantMap ArchipelagoConfig::anchors() const { return m_anchors; }
QVariantMap ArchipelagoConfig::modules() const { return m_modules; }
QVariantMap ArchipelagoConfig::styleOverrides() const { return m_styleOverrides; }
QString ArchipelagoConfig::iconFontFamily() const { return m_iconFontFamily; }
QString ArchipelagoConfig::textFontFamily() const { return m_textFontFamily; }
QString ArchipelagoConfig::timeFontFamily() const { return m_timeFontFamily; }
int ArchipelagoConfig::bodyFontSize() const { return m_bodyFontSize; }
int ArchipelagoConfig::titleFontSize() const { return m_titleFontSize; }
int ArchipelagoConfig::iconFontSize() const { return m_iconFontSize; }
int ArchipelagoConfig::islandHeight() const { return m_islandHeight; }
int ArchipelagoConfig::islandGap() const { return m_islandGap; }
int ArchipelagoConfig::edgeMargin() const { return m_edgeMargin; }
int ArchipelagoConfig::expandedVerticalOffset() const { return m_expandedVerticalOffset; }
int ArchipelagoConfig::expandedWidth() const { return m_expandedWidth; }
int ArchipelagoConfig::expandedHeight() const { return m_expandedHeight; }
int ArchipelagoConfig::morphDuration() const { return m_morphDuration; }
int ArchipelagoConfig::compactFadeDuration() const { return m_compactFadeDuration; }
int ArchipelagoConfig::contentFadeDuration() const { return m_contentFadeDuration; }
QString ArchipelagoConfig::compositor() const { return m_compositor; }

QVariantList ArchipelagoConfig::anchorModules(const QString &anchor) const
{
    return m_anchors.value(anchor).toList();
}

QVariant ArchipelagoConfig::moduleConfig(const QString &moduleId) const
{
    return m_modules.value(moduleId);
}

bool ArchipelagoConfig::moduleEnabled(const QString &moduleId) const
{
    return variantMapValue(m_modules, moduleId).value(QStringLiteral("enabled"), true).toBool();
}

int ArchipelagoConfig::modulePriority(const QString &moduleId) const
{
    return variantMapValue(m_modules, moduleId).value(QStringLiteral("priority"), 0).toInt();
}

QString ArchipelagoConfig::moduleCompactMode(const QString &moduleId) const
{
    return variantMapValue(m_modules, moduleId).value(QStringLiteral("compact")).toString();
}

QString ArchipelagoConfig::moduleAction(const QString &moduleId, const QString &actionName) const
{
    const QVariantMap actions = variantMapValue(variantMapValue(m_modules, moduleId), QStringLiteral("actions"));
    return actions.value(actionName).toString();
}

QString ArchipelagoConfig::moduleScrollAction(const QString &moduleId, const QString &direction) const
{
    const QVariantMap scroll = variantMapValue(variantMapValue(m_modules, moduleId), QStringLiteral("scroll"));
    return scroll.value(direction).toString();
}

QString ArchipelagoConfig::styleOverride(const QString &tokenName) const
{
    return m_styleOverrides.value(tokenName).toString();
}

int ArchipelagoConfig::mouseButton(const QVariant &button) const
{
    bool ok = false;
    const int numericButton = button.toInt(&ok);
    if (!ok)
        return Qt::NoButton;

    switch (numericButton) {
    case 1:
        return Qt::LeftButton;
    case 2:
        return Qt::MiddleButton;
    case 3:
        return Qt::RightButton;
    default:
        return numericButton;
    }
}

int ArchipelagoConfig::mouseButtonsMask(const QVariant &buttons) const
{
    if (!buttons.isValid() || buttons.isNull())
        return Qt::NoButton;

    if (buttons.canConvert<QVariantList>()) {
        int mask = Qt::NoButton;
        const QVariantList buttonList = buttons.toList();
        for (const QVariant &button : buttonList)
            mask |= mouseButton(button);
        return mask;
    }

    return mouseButton(buttons);
}

void ArchipelagoConfig::reload()
{
    loadConfig();
}

void ArchipelagoConfig::scheduleReload()
{
    if (!m_reloadTimer.isActive())
        m_reloadTimer.start();
}

void ArchipelagoConfig::loadConfig()
{
    updateWatchedPaths();

    QJsonObject configObject;
    QStringList errors;

    QFile configFile(m_userConfigPath);
    if (configFile.exists()) {
        if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            errors.append(QStringLiteral("Could not read %1: %2").arg(m_userConfigPath, configFile.errorString()));
        } else {
            const QByteArray configBytes = configFile.readAll();
            if (!configBytes.trimmed().isEmpty()) {
                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(stripJsonComments(configBytes), &parseError);
                if (parseError.error != QJsonParseError::NoError) {
                    errors.append(QStringLiteral("Invalid JSON in %1 at offset %2: %3")
                                      .arg(m_userConfigPath)
                                      .arg(parseError.offset)
                                      .arg(parseError.errorString()));
                } else if (!document.isObject()) {
                    errors.append(QStringLiteral("Invalid JSON in %1: root value must be an object").arg(m_userConfigPath));
                } else {
                    configObject = document.object();
                }
            }
        }
    }

    applyLoadedConfig(parseConfigObject(configObject, errors));
    updateWatchedPaths();
}

ArchipelagoConfig::LoadedConfig ArchipelagoConfig::parseConfigObject(const QJsonObject &configObject,
                                                                     const QStringList &initialErrors) const
{
    LoadedConfig next;
    next.anchors = defaultAnchors();
    next.modules = defaultModules();
    next.iconFontFamily = QStringLiteral("JetBrainsMono Nerd Font");
    next.textFontFamily = QStringLiteral("Inter Display");
    next.timeFontFamily = QStringLiteral("Inter Display");
    next.compositor = QStringLiteral("auto");

    QStringList errors = initialErrors;
    if (!initialErrors.isEmpty()) {
        next.error = initialErrors.join(QStringLiteral(" "));
        return next;
    }

    const QJsonValue anchorsValue = configObject.value(QStringLiteral("anchors"));
    if (!anchorsValue.isUndefined()) {
        if (!anchorsValue.isObject()) {
            errors.append(QStringLiteral("Invalid anchors; using defaults."));
        } else {
            const QJsonObject anchorsObject = anchorsValue.toObject();
            const auto keys = anchorsObject.keys();
            for (const QString &anchor : keys) {
                if (!isAllowedAnchor(anchor)) {
                    errors.append(QStringLiteral("Invalid anchors.%1; unknown anchor.").arg(anchor));
                    continue;
                }

                const QJsonValue value = anchorsObject.value(anchor);
                if (!value.isArray()) {
                    errors.append(QStringLiteral("Invalid anchors.%1; using default.").arg(anchor));
                    continue;
                }

                QVariantList modulesForAnchor;
                const QJsonArray array = value.toArray();
                for (const QJsonValue &entry : array) {
                    if (!entry.isString() || entry.toString().trimmed().isEmpty()) {
                        errors.append(QStringLiteral("Invalid anchors.%1 item; ignoring it.").arg(anchor));
                        continue;
                    }
                    modulesForAnchor.append(entry.toString());
                }
                next.anchors.insert(anchor, modulesForAnchor);
            }
        }
    }

    const QJsonValue modulesValue = configObject.value(QStringLiteral("modules"));
    if (!modulesValue.isUndefined()) {
        if (!modulesValue.isObject()) {
            errors.append(QStringLiteral("Invalid modules; using defaults."));
        } else {
            const QJsonObject modulesObject = modulesValue.toObject();
            const auto keys = modulesObject.keys();
            for (const QString &moduleId : keys) {
                if (moduleId.trimmed().isEmpty()) {
                    errors.append(QStringLiteral("Invalid modules entry; empty module id."));
                    continue;
                }

                const QJsonValue value = modulesObject.value(moduleId);
                if (!value.isObject()) {
                    errors.append(QStringLiteral("Invalid modules.%1; using default.").arg(moduleId));
                    continue;
                }

                QVariantMap module = next.modules.contains(moduleId)
                    ? next.modules.value(moduleId).toMap()
                    : defaultModuleConfig();
                const QJsonObject moduleObject = value.toObject();
                module.insert(QStringLiteral("enabled"),
                              jsonBool(moduleObject,
                                       QStringLiteral("enabled"),
                                       module.value(QStringLiteral("enabled")).toBool(),
                                       &errors,
                                       QStringLiteral("modules.%1.enabled").arg(moduleId)));
                module.insert(QStringLiteral("priority"),
                              jsonIntRange(moduleObject,
                                           QStringLiteral("priority"),
                                           module.value(QStringLiteral("priority")).toInt(),
                                           0,
                                           100,
                                           &errors,
                                           QStringLiteral("modules.%1.priority").arg(moduleId)));
                module.insert(QStringLiteral("compact"),
                              jsonString(moduleObject,
                                         QStringLiteral("compact"),
                                         module.value(QStringLiteral("compact")).toString(),
                                         &errors,
                                         QStringLiteral("modules.%1.compact").arg(moduleId)));
                if (moduleObject.contains(QStringLiteral("width")) || module.contains(QStringLiteral("width"))) {
                    const int fallbackWidth = module.contains(QStringLiteral("width"))
                        ? module.value(QStringLiteral("width")).toInt()
                        : 0;
                    const int width = jsonIntRange(moduleObject,
                                                   QStringLiteral("width"),
                                                   fallbackWidth,
                                                   44,
                                                   360,
                                                   &errors,
                                                   QStringLiteral("modules.%1.width").arg(moduleId));
                    if (width > 0) {
                        module.insert(QStringLiteral("width"), width);
                    } else {
                        module.remove(QStringLiteral("width"));
                    }
                }

                const auto mergeStringMap = [&errors, &moduleObject, &moduleId](const QString &key, QVariantMap fallback) {
                    const QJsonValue value = moduleObject.value(key);
                    if (value.isUndefined())
                        return fallback;
                    if (!value.isObject()) {
                        errors.append(QStringLiteral("Invalid modules.%1.%2; using default.").arg(moduleId, key));
                        return fallback;
                    }

                    const QJsonObject object = value.toObject();
                    const auto keys = object.keys();
                    for (const QString &entryKey : keys) {
                        const QJsonValue entry = object.value(entryKey);
                        if (!entry.isString() || entry.toString().trimmed().isEmpty()) {
                            errors.append(QStringLiteral("Invalid modules.%1.%2.%3; ignoring it.")
                                              .arg(moduleId, key, entryKey));
                            continue;
                        }
                        fallback.insert(entryKey, entry.toString());
                    }
                    return fallback;
                };

                module.insert(QStringLiteral("actions"),
                              mergeStringMap(QStringLiteral("actions"), module.value(QStringLiteral("actions")).toMap()));
                module.insert(QStringLiteral("scroll"),
                              mergeStringMap(QStringLiteral("scroll"), module.value(QStringLiteral("scroll")).toMap()));
                next.modules.insert(moduleId, module);
            }
        }
    }

    const QJsonValue fontsValue = configObject.value(QStringLiteral("fonts"));
    if (!fontsValue.isUndefined()) {
        if (!fontsValue.isObject()) {
            errors.append(QStringLiteral("Invalid fonts; using defaults."));
        } else {
            const QJsonObject fonts = fontsValue.toObject();
            next.textFontFamily = jsonString(fonts, QStringLiteral("text"), next.textFontFamily, &errors, QStringLiteral("fonts.text"));
            next.iconFontFamily = jsonString(fonts, QStringLiteral("icon"), next.iconFontFamily, &errors, QStringLiteral("fonts.icon"));
            next.timeFontFamily = jsonString(fonts, QStringLiteral("time"), next.timeFontFamily, &errors, QStringLiteral("fonts.time"));
            next.bodyFontSize = jsonIntRange(fonts, QStringLiteral("bodySize"), next.bodyFontSize, 8, 32, &errors, QStringLiteral("fonts.bodySize"));
            next.titleFontSize = jsonIntRange(fonts, QStringLiteral("titleSize"), next.titleFontSize, 10, 40, &errors, QStringLiteral("fonts.titleSize"));
            next.iconFontSize = jsonIntRange(fonts, QStringLiteral("iconSize"), next.iconFontSize, 10, 40, &errors, QStringLiteral("fonts.iconSize"));
        }
    }

    const QJsonValue sizesValue = configObject.value(QStringLiteral("sizes"));
    if (!sizesValue.isUndefined()) {
        if (!sizesValue.isObject()) {
            errors.append(QStringLiteral("Invalid sizes; using defaults."));
        } else {
            const QJsonObject sizes = sizesValue.toObject();
            next.islandHeight = jsonIntRange(sizes, QStringLiteral("islandHeight"), next.islandHeight, 24, 72, &errors, QStringLiteral("sizes.islandHeight"));
            next.islandGap = jsonIntRange(sizes, QStringLiteral("islandGap"), next.islandGap, 0, 40, &errors, QStringLiteral("sizes.islandGap"));
            next.edgeMargin = jsonIntRange(sizes, QStringLiteral("edgeMargin"), next.edgeMargin, 0, 96, &errors, QStringLiteral("sizes.edgeMargin"));
            next.expandedVerticalOffset = jsonIntRange(sizes,
                                                       QStringLiteral("expandedVerticalOffset"),
                                                       next.expandedVerticalOffset,
                                                       6,
                                                       60,
                                                       &errors,
                                                       QStringLiteral("sizes.expandedVerticalOffset"));
            next.expandedWidth = jsonIntRange(sizes, QStringLiteral("expandedWidth"), next.expandedWidth, 320, 1200, &errors, QStringLiteral("sizes.expandedWidth"));
            next.expandedHeight = jsonIntRange(sizes, QStringLiteral("expandedHeight"), next.expandedHeight, 220, 900, &errors, QStringLiteral("sizes.expandedHeight"));
        }
    }

    const QJsonValue animationsValue = configObject.value(QStringLiteral("animations"));
    if (!animationsValue.isUndefined()) {
        if (!animationsValue.isObject()) {
            errors.append(QStringLiteral("Invalid animations; using defaults."));
        } else {
            const QJsonObject animations = animationsValue.toObject();
            next.morphDuration = jsonIntRange(animations, QStringLiteral("morphMs"), next.morphDuration, 160, 700, &errors, QStringLiteral("animations.morphMs"));
            next.compactFadeDuration = jsonIntRange(animations,
                                                    QStringLiteral("compactFadeMs"),
                                                    next.compactFadeDuration,
                                                    80,
                                                    320,
                                                    &errors,
                                                    QStringLiteral("animations.compactFadeMs"));
            next.contentFadeDuration = jsonIntRange(animations,
                                                    QStringLiteral("contentFadeMs"),
                                                    next.contentFadeDuration,
                                                    80,
                                                    360,
                                                    &errors,
                                                    QStringLiteral("animations.contentFadeMs"));
        }
    }

    const QJsonValue styleValue = configObject.value(QStringLiteral("style"));
    if (!styleValue.isUndefined()) {
        if (!styleValue.isObject()) {
            errors.append(QStringLiteral("Invalid style; using defaults."));
        } else {
            const QJsonObject style = styleValue.toObject();
            const auto keys = style.keys();
            for (const QString &key : keys) {
                const QJsonValue value = style.value(key);
                if (!value.isString() || !QColor(value.toString()).isValid()) {
                    errors.append(QStringLiteral("Invalid style.%1; ignoring it.").arg(key));
                    continue;
                }
                next.styleOverrides.insert(key, value.toString());
            }
        }
    }

    const QJsonValue compositorValue = configObject.value(QStringLiteral("compositor"));
    if (!compositorValue.isUndefined()) {
        if (!compositorValue.isString()) {
            errors.append(QStringLiteral("Invalid compositor; using default."));
        } else {
            const QString compositor = compositorValue.toString().trimmed().toLower();
            if (!isAllowedCompositor(compositor))
                errors.append(QStringLiteral("Invalid compositor; using default."));
            else
                next.compositor = compositor;
        }
    }

    next.error = errors.join(QStringLiteral(" "));
    return next;
}

void ArchipelagoConfig::applyLoadedConfig(const LoadedConfig &next)
{
    bool changed = false;
    changed |= assignIfChanged(m_anchors, next.anchors);
    changed |= assignIfChanged(m_modules, next.modules);
    changed |= assignIfChanged(m_styleOverrides, next.styleOverrides);
    changed |= assignIfChanged(m_iconFontFamily, next.iconFontFamily);
    changed |= assignIfChanged(m_textFontFamily, next.textFontFamily);
    changed |= assignIfChanged(m_timeFontFamily, next.timeFontFamily);
    changed |= assignIfChanged(m_bodyFontSize, next.bodyFontSize);
    changed |= assignIfChanged(m_titleFontSize, next.titleFontSize);
    changed |= assignIfChanged(m_iconFontSize, next.iconFontSize);
    changed |= assignIfChanged(m_islandHeight, next.islandHeight);
    changed |= assignIfChanged(m_islandGap, next.islandGap);
    changed |= assignIfChanged(m_edgeMargin, next.edgeMargin);
    changed |= assignIfChanged(m_expandedVerticalOffset, next.expandedVerticalOffset);
    changed |= assignIfChanged(m_expandedWidth, next.expandedWidth);
    changed |= assignIfChanged(m_expandedHeight, next.expandedHeight);
    changed |= assignIfChanged(m_morphDuration, next.morphDuration);
    changed |= assignIfChanged(m_compactFadeDuration, next.compactFadeDuration);
    changed |= assignIfChanged(m_contentFadeDuration, next.contentFadeDuration);
    changed |= assignIfChanged(m_compositor, next.compositor);

    if (m_configError != next.error) {
        m_configError = next.error;
        emit configErrorChanged();
    }

    if (changed)
        emit configChanged();
}

void ArchipelagoConfig::updateWatchedPaths()
{
    const QString configDirectory = QFileInfo(m_userConfigPath).absolutePath();
    const QString configParentDirectory = QFileInfo(configDirectory).absolutePath();
    const QSet<QString> wantedFiles = QFileInfo::exists(m_userConfigPath)
        ? QSet<QString>{m_userConfigPath}
        : QSet<QString>{};
    QSet<QString> wantedDirectories;
    if (QFileInfo::exists(configParentDirectory))
        wantedDirectories.insert(configParentDirectory);
    if (QFileInfo::exists(configDirectory))
        wantedDirectories.insert(configDirectory);

    const QStringList currentFiles = m_watcher.files();
    for (const QString &path : currentFiles) {
        if (!wantedFiles.contains(path))
            m_watcher.removePath(path);
    }

    const QStringList currentDirectories = m_watcher.directories();
    for (const QString &path : currentDirectories) {
        if (!wantedDirectories.contains(path))
            m_watcher.removePath(path);
    }

    for (const QString &path : wantedFiles) {
        if (!m_watcher.files().contains(path))
            m_watcher.addPath(path);
    }

    for (const QString &path : wantedDirectories) {
        if (!m_watcher.directories().contains(path))
            m_watcher.addPath(path);
    }
}

QString ArchipelagoConfig::configHome() const
{
    const QByteArray xdgConfigHome = qgetenv("XDG_CONFIG_HOME");
    if (!xdgConfigHome.isEmpty())
        return QString::fromLocal8Bit(xdgConfigHome);

    const QByteArray home = qgetenv("HOME");
    return home.isEmpty() ? QStringLiteral(".") : QString::fromLocal8Bit(home) + QStringLiteral("/.config");
}
