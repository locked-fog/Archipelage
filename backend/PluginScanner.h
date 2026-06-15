#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

// Discovers business-module plugins under the configured plugins base
// directory and exposes their manifests to QML.
//
// Plugin directory layout (one directory per plugin):
//   <pluginsBase>/<id>/
//     manifest.json     (optional — see schema below)
//     Compact.qml       (optional)
//     Expanded.qml      (optional)
//
// manifest.json schema (all fields optional):
//   {
//     "id":              "workspaces",        // defaults to directory name
//     "label":           "Workspaces",        // human-readable
//     "version":         "1.0.0",
//     "anchors":         ["left", "center"],  // hint for the host
//     "defaultPriority": 90,                  // used when config omits priority
//     "compact":         "Compact.qml",       // defaults to "Compact.qml"
//     "expanded":        "Expanded.qml",      // defaults to "Expanded.qml"
//     "preferredWidth":  680,                 // 0 → ArchipelagoConfig default
//     "preferredHeight": 430,
//     "dataNeeds":       ["niriService"]      // informational; plugin author
//                                              // is responsible for importing
//                                              // the actual backend singletons
//   }
//
// The exposed entry always includes both:
//   id            stable plugin id consumed by config / registry
//   directoryName actual directory used to resolve Compact.qml / Expanded.qml
//
// If manifest.json is missing or unparseable the scanner falls back to
// the directory-name + "Compact.qml"/"Expanded.qml" convention so that
// dropping a new directory under pluginsBase is enough to register a
// plugin.
//
// Search path resolution (first existing path wins):
//   1. $ARCHIPELAGO_PLUGINS_DIR
//   2. $PWD/qml/plugins/                (dev runs from project root)
//   3. <install prefix>/share/archipelago/qml/plugins
class PluginScanner : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged FINAL)
    Q_PROPERTY(QString pluginsBase READ pluginsBase NOTIFY pluginsBaseChanged FINAL)
    Q_PROPERTY(QStringList searchPaths READ searchPaths CONSTANT FINAL)

public:
    explicit PluginScanner(QObject *parent = nullptr);
    ~PluginScanner() override;

    QVariantList plugins() const;
    QString pluginsBase() const;
    QStringList searchPaths() const;

    Q_INVOKABLE QVariantMap manifestFor(const QString &id) const;
    Q_INVOKABLE void rescan();

signals:
    void pluginsChanged();
    void pluginsBaseChanged();

private:
    void scan();
    QVariantMap parsePluginDir(const QString &absoluteDirPath) const;
    QVariantMap parseManifestFile(const QString &manifestPath,
                                  const QString &fallbackId) const;
    QString resolvePluginsBase() const;
    QStringList listSubdirectories(const QString &absoluteDirPath) const;

    QVariantList m_plugins;
    QString m_pluginsBase;
};
