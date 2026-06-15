#include "PluginScanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcPlugins, "archipelago.plugins")

constexpr const char *kManifestFile = "manifest.json";
constexpr const char *kCompactFile = "Compact.qml";
constexpr const char *kExpandedFile = "Expanded.qml";

QStringList pluginSearchPaths()
{
    QStringList paths;

    const QByteArray envOverride = qgetenv("ARCHIPELAGO_PLUGINS_DIR");
    if (!envOverride.isEmpty())
        paths << QString::fromLocal8Bit(envOverride);

    paths << QDir::currentPath() + QStringLiteral("/qml/plugins");

    // The CMake install rule puts the qml tree under
    // <prefix>/share/archipelago/qml. We resolve <prefix> via the
    // binary location and fall back to QStandardPaths.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList installCandidates = {
        appDir + QStringLiteral("/../share/archipelago/qml/plugins"),
        appDir + QStringLiteral("/../../share/archipelago/qml/plugins"),
        QStringLiteral("/usr/share/archipelago/qml/plugins"),
        QStringLiteral("/usr/local/share/archipelago/qml/plugins")
    };
    for (const QString &candidate : installCandidates)
        paths << candidate;

    return paths;
}

QVariantMap fallbackEntry(const QString &absoluteDirPath, const QString &id)
{
    const QString compactPath = absoluteDirPath + QLatin1Char('/') + QLatin1String(kCompactFile);
    const QString expandedPath = absoluteDirPath + QLatin1Char('/') + QLatin1String(kExpandedFile);

    QVariantMap entry;
    entry.insert(QStringLiteral("id"), id);
    entry.insert(QStringLiteral("label"), id);
    entry.insert(QStringLiteral("version"), QStringLiteral("0.0.0"));
    entry.insert(QStringLiteral("anchors"), QVariantList());
    entry.insert(QStringLiteral("defaultPriority"), 0);
    entry.insert(QStringLiteral("compact"), QFile::exists(compactPath) ? QLatin1String(kCompactFile) : QString());
    entry.insert(QStringLiteral("expanded"), QFile::exists(expandedPath) ? QLatin1String(kExpandedFile) : QString());
    entry.insert(QStringLiteral("preferredWidth"), 0);
    entry.insert(QStringLiteral("preferredHeight"), 0);
    entry.insert(QStringLiteral("dataNeeds"), QVariantList());
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
    if (!m_pluginsBase.isEmpty()) {
        const QStringList subdirs = listSubdirectories(m_pluginsBase);
        for (const QString &subdir : subdirs)
            next.append(parsePluginDir(subdir));
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
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), object.value(QStringLiteral("id")).toString(fallbackId));
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
                 object.value(QStringLiteral("compact")).toString(QStringLiteral("Compact.qml")));
    entry.insert(QStringLiteral("expanded"),
                 object.value(QStringLiteral("expanded")).toString(QStringLiteral("Expanded.qml")));
    entry.insert(QStringLiteral("preferredWidth"),
                 object.value(QStringLiteral("preferredWidth")).toInt(0));
    entry.insert(QStringLiteral("preferredHeight"),
                 object.value(QStringLiteral("preferredHeight")).toInt(0));

    const QJsonValue dataNeeds = object.value(QStringLiteral("dataNeeds"));
    entry.insert(QStringLiteral("dataNeeds"),
                 dataNeeds.isArray() ? dataNeeds.toArray().toVariantList() : QVariantList());
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
