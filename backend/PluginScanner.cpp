#include "PluginScanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcPlugins, "archipelago.plugins")

constexpr const char *kManifestFile = "manifest.json";
constexpr const char *kCompactFile = "Compact.qml";
constexpr const char *kExpandedFile = "Expanded.qml";

bool isSafePluginId(const QString &id)
{
    if (id.isEmpty())
        return false;
    for (const QChar ch : id) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')
            || ch == QLatin1Char('-') || ch == QLatin1Char('.'))
            continue;
        return false;
    }
    return true;
}

QString safeComponentFileName(const QJsonObject &object,
                              const QString &key,
                              const QString &fallback)
{
    const QString value = object.value(key).toString(fallback).trimmed();
    if (value.isEmpty())
        return QString();
    if (value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('\\'))) {
        qCWarning(lcPlugins) << "Ignoring unsafe component path in manifest key" << key << ":" << value;
        return fallback;
    }
    if (!value.endsWith(QStringLiteral(".qml"))) {
        qCWarning(lcPlugins) << "Ignoring non-QML component path in manifest key" << key << ":" << value;
        return fallback;
    }
    return value;
}

QString safePreviewComponentFileName(const QJsonObject &object, const QString &key)
{
    const QString value = object.value(key).toString().trimmed();
    if (value.isEmpty())
        return QString();
    if (value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('\\'))) {
        qCWarning(lcPlugins) << "Ignoring unsafe preview component path in manifest key" << key << ":" << value;
        return QString();
    }
    if (!value.endsWith(QStringLiteral(".qml"))) {
        qCWarning(lcPlugins) << "Ignoring non-QML preview component path in manifest key" << key << ":" << value;
        return QString();
    }
    return value;
}

QVariantList parsePreviewTemplates(const QJsonObject &object)
{
    QVariantList result;
    const QJsonValue templates = object.value(QStringLiteral("previewTemplates"));
    if (!templates.isArray())
        return result;

    const QJsonArray array = templates.toArray();
    for (const QJsonValue &entryValue : array) {
        if (!entryValue.isObject()) {
            qCWarning(lcPlugins) << "Ignoring non-object preview template";
            continue;
        }

        const QJsonObject entryObject = entryValue.toObject();
        const QString id = entryObject.value(QStringLiteral("id")).toString().trimmed();
        const QString component = safePreviewComponentFileName(entryObject, QStringLiteral("component"));
        if (!isSafePluginId(id) || component.isEmpty()) {
            qCWarning(lcPlugins) << "Ignoring invalid preview template" << id;
            continue;
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("component"), component);
        entry.insert(QStringLiteral("defaultWidth"), entryObject.value(QStringLiteral("defaultWidth")).toInt(360));
        entry.insert(QStringLiteral("defaultHeight"), entryObject.value(QStringLiteral("defaultHeight")).toInt(112));
        entry.insert(QStringLiteral("timeoutMs"), entryObject.value(QStringLiteral("timeoutMs")).toInt(5000));
        entry.insert(QStringLiteral("maxVisible"), entryObject.value(QStringLiteral("maxVisible")).toInt(5));
        entry.insert(QStringLiteral("focusPolicy"), entryObject.value(QStringLiteral("focusPolicy")).toString(QStringLiteral("passive")));
        result.append(entry);
    }
    return result;
}

QStringList pluginSearchPaths()
{
    QStringList paths;

    const QByteArray envOverride = qgetenv("ARCHIPELAGO_PLUGINS_DIR");
    if (!envOverride.isEmpty()) {
        paths << QString::fromLocal8Bit(envOverride);
        return paths;
    }

    const QByteArray xdgDataHome = qgetenv("XDG_DATA_HOME");
    const QString userDataHome = !xdgDataHome.isEmpty()
        ? QString::fromLocal8Bit(xdgDataHome)
        : QDir::homePath() + QStringLiteral("/.local/share");
    paths << userDataHome + QStringLiteral("/archipelago/plugins");

    const QByteArray xdgDataDirs = qgetenv("XDG_DATA_DIRS");
    const QStringList systemDataDirs = !xdgDataDirs.isEmpty()
        ? QString::fromLocal8Bit(xdgDataDirs).split(QLatin1Char(':'), Qt::SkipEmptyParts)
        : QStringList({
              QStringLiteral("/usr/share"),
              QStringLiteral("/usr/local/share")
          });
    for (const QString &dataDir : systemDataDirs)
        paths << dataDir + QStringLiteral("/archipelago/plugins");

    paths << QDir::currentPath() + QStringLiteral("/qml/plugins");

    // The CMake install rule puts the qml tree under
    // <prefix>/share/archipelago/qml. We resolve <prefix> via the
    // binary location and fall back to well-known system install roots.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList installCandidates = {
        appDir + QStringLiteral("/../share/archipelago/qml/plugins"),
        appDir + QStringLiteral("/../../share/archipelago/qml/plugins"),
        QStringLiteral("/usr/share/archipelago/qml/plugins"),
        QStringLiteral("/usr/local/share/archipelago/qml/plugins")
    };
    for (const QString &candidate : installCandidates)
        paths << candidate;

    QStringList uniquePaths;
    QSet<QString> seen;
    for (const QString &path : paths) {
        const QString cleaned = QDir::cleanPath(path);
        if (cleaned.isEmpty() || seen.contains(cleaned))
            continue;
        seen.insert(cleaned);
        uniquePaths << cleaned;
    }
    return uniquePaths;
}

QVariantMap fallbackEntry(const QString &absoluteDirPath, const QString &id)
{
    if (!isSafePluginId(id)) {
        qCWarning(lcPlugins) << "Ignoring plugin directory with unsafe id:" << id;
        return {};
    }

    const QString compactPath = absoluteDirPath + QLatin1Char('/') + QLatin1String(kCompactFile);
    const QString expandedPath = absoluteDirPath + QLatin1Char('/') + QLatin1String(kExpandedFile);

    QVariantMap entry;
    entry.insert(QStringLiteral("id"), id);
    entry.insert(QStringLiteral("directoryName"), id);
    entry.insert(QStringLiteral("directoryPath"), absoluteDirPath);
    entry.insert(QStringLiteral("label"), id);
    entry.insert(QStringLiteral("version"), QStringLiteral("0.0.0"));
    entry.insert(QStringLiteral("anchors"), QVariantList());
    entry.insert(QStringLiteral("defaultPriority"), 0);
    entry.insert(QStringLiteral("compact"), QFile::exists(compactPath) ? QLatin1String(kCompactFile) : QString());
    entry.insert(QStringLiteral("expanded"), QFile::exists(expandedPath) ? QLatin1String(kExpandedFile) : QString());
    entry.insert(QStringLiteral("preferredWidth"), 0);
    entry.insert(QStringLiteral("preferredHeight"), 0);
    entry.insert(QStringLiteral("dataNeeds"), QVariantList());
    entry.insert(QStringLiteral("previewTemplates"), QVariantList());
    entry.insert(QStringLiteral("source"), QStringLiteral("fallback"));
    return entry;
}
}  // namespace

PluginScanner::PluginScanner(QObject *parent)
    : QObject(parent)
{
    m_pluginsBase = resolvePluginsBase();
    if (!m_pluginsBase.isEmpty())
        scan();
}

PluginScanner::~PluginScanner() = default;

QVariantList PluginScanner::plugins() const { return m_plugins; }
QString PluginScanner::pluginsBase() const { return m_pluginsBase; }
QStringList PluginScanner::searchPaths() const { return pluginSearchPaths(); }

QVariantMap PluginScanner::manifestFor(const QString &id) const
{
    for (const QVariant &entry : m_plugins) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == id)
            return map;
    }
    return {};
}

void PluginScanner::rescan()
{
    const QString previous = m_pluginsBase;
    m_pluginsBase = resolvePluginsBase();
    if (m_pluginsBase != previous)
        emit pluginsBaseChanged();
    scan();
}

void PluginScanner::scan()
{
    QVariantList next;
    QSet<QString> seenIds;

    for (const QString &pluginsRoot : pluginSearchPaths()) {
        const QFileInfo rootInfo(pluginsRoot);
        if (!rootInfo.exists() || !rootInfo.isDir())
            continue;

        const QStringList subdirs = listSubdirectories(rootInfo.absoluteFilePath());
        for (const QString &subdir : subdirs) {
            const QVariantMap entry = parsePluginDir(subdir);
            const QString id = entry.value(QStringLiteral("id")).toString();
            if (entry.isEmpty() || seenIds.contains(id))
                continue;
            seenIds.insert(id);
            next.append(entry);
        }
    }

    m_plugins = next;
    emit pluginsChanged();
}

QVariantMap PluginScanner::parsePluginDir(const QString &absoluteDirPath) const
{
    const QFileInfo dirInfo(absoluteDirPath);
    if (!dirInfo.exists() || !dirInfo.isDir())
        return {};

    const QString id = dirInfo.fileName();
    const QString manifestPath = absoluteDirPath + QLatin1Char('/') + QLatin1String(kManifestFile);

    if (QFile::exists(manifestPath))
        return parseManifestFile(manifestPath, id);

    return fallbackEntry(absoluteDirPath, id);
}

QVariantMap PluginScanner::parseManifestFile(const QString &manifestPath,
                                             const QString &fallbackId) const
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcPlugins) << "Failed to open manifest:" << manifestPath;
        return fallbackEntry(QFileInfo(manifestPath).absolutePath(), fallbackId);
    }

    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(lcPlugins) << "Invalid manifest JSON in" << manifestPath
                             << ":" << error.errorString();
        return fallbackEntry(QFileInfo(manifestPath).absolutePath(), fallbackId);
    }

    const QJsonObject object = document.object();
    const QString requestedId = object.value(QStringLiteral("id")).toString(fallbackId).trimmed();
    const QString id = isSafePluginId(requestedId) ? requestedId : fallbackId;
    if (!isSafePluginId(id)) {
        qCWarning(lcPlugins) << "Ignoring manifest with unsafe id:" << requestedId
                             << "in" << manifestPath;
        return {};
    }
    if (id != requestedId) {
        qCWarning(lcPlugins) << "Manifest id is unsafe; using directory name for" << manifestPath;
    }

    QVariantMap entry;
    entry.insert(QStringLiteral("id"), id);
    entry.insert(QStringLiteral("directoryName"), fallbackId);
    entry.insert(QStringLiteral("directoryPath"), QFileInfo(manifestPath).absolutePath());
    entry.insert(QStringLiteral("label"),
                 object.value(QStringLiteral("label")).toString(entry.value(QStringLiteral("id")).toString()));
    entry.insert(QStringLiteral("version"),
                 object.value(QStringLiteral("version")).toString(QStringLiteral("0.0.0")));

    const QJsonValue anchors = object.value(QStringLiteral("anchors"));
    entry.insert(QStringLiteral("anchors"),
                 anchors.isArray() ? anchors.toArray().toVariantList() : QVariantList());

    entry.insert(QStringLiteral("defaultPriority"),
                 object.value(QStringLiteral("defaultPriority")).toInt(0));
    entry.insert(QStringLiteral("compact"),
                 safeComponentFileName(object, QStringLiteral("compact"), QStringLiteral("Compact.qml")));
    entry.insert(QStringLiteral("expanded"),
                 safeComponentFileName(object, QStringLiteral("expanded"), QStringLiteral("Expanded.qml")));
    entry.insert(QStringLiteral("preferredWidth"),
                 object.value(QStringLiteral("preferredWidth")).toInt(0));
    entry.insert(QStringLiteral("preferredHeight"),
                 object.value(QStringLiteral("preferredHeight")).toInt(0));

    const QJsonValue dataNeeds = object.value(QStringLiteral("dataNeeds"));
    entry.insert(QStringLiteral("dataNeeds"),
                 dataNeeds.isArray() ? dataNeeds.toArray().toVariantList() : QVariantList());
    entry.insert(QStringLiteral("previewTemplates"), parsePreviewTemplates(object));
    entry.insert(QStringLiteral("source"), QStringLiteral("manifest"));
    return entry;
}

QString PluginScanner::resolvePluginsBase() const
{
    for (const QString &candidate : pluginSearchPaths()) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isDir())
            return info.absoluteFilePath();
    }
    return QString();
}

QStringList PluginScanner::listSubdirectories(const QString &absoluteDirPath) const
{
    QDir dir(absoluteDirPath);
    if (!dir.exists())
        return {};

    QStringList result;
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo &entry : entries)
        result.append(entry.absoluteFilePath());
    return result;
}
