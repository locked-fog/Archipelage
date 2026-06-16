#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

class QJsonObject;

class ArchipelagoConfig final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ArchipelagoConfig)
    QML_SINGLETON

    Q_PROPERTY(QString userConfigPath READ userConfigPath CONSTANT FINAL)
    Q_PROPERTY(QString configError READ configError NOTIFY configErrorChanged FINAL)
    Q_PROPERTY(QVariantMap anchors READ anchors NOTIFY configChanged FINAL)
    Q_PROPERTY(QVariantMap modules READ modules NOTIFY configChanged FINAL)
    Q_PROPERTY(QVariantMap styleOverrides READ styleOverrides NOTIFY configChanged FINAL)
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily NOTIFY configChanged FINAL)
    Q_PROPERTY(QString textFontFamily READ textFontFamily NOTIFY configChanged FINAL)
    Q_PROPERTY(QString timeFontFamily READ timeFontFamily NOTIFY configChanged FINAL)
    Q_PROPERTY(int bodyFontSize READ bodyFontSize NOTIFY configChanged FINAL)
    Q_PROPERTY(int titleFontSize READ titleFontSize NOTIFY configChanged FINAL)
    Q_PROPERTY(int iconFontSize READ iconFontSize NOTIFY configChanged FINAL)
    Q_PROPERTY(int islandHeight READ islandHeight NOTIFY configChanged FINAL)
    Q_PROPERTY(int islandGap READ islandGap NOTIFY configChanged FINAL)
    Q_PROPERTY(int edgeMargin READ edgeMargin NOTIFY configChanged FINAL)
    Q_PROPERTY(int expandedVerticalOffset READ expandedVerticalOffset NOTIFY configChanged FINAL)
    Q_PROPERTY(int expandedWidth READ expandedWidth NOTIFY configChanged FINAL)
    Q_PROPERTY(int expandedHeight READ expandedHeight NOTIFY configChanged FINAL)
    Q_PROPERTY(int morphDuration READ morphDuration NOTIFY configChanged FINAL)
    Q_PROPERTY(int compactFadeDuration READ compactFadeDuration NOTIFY configChanged FINAL)
    Q_PROPERTY(int contentFadeDuration READ contentFadeDuration NOTIFY configChanged FINAL)
    Q_PROPERTY(QString compositor READ compositor NOTIFY configChanged FINAL)

public:
    explicit ArchipelagoConfig(QObject *parent = nullptr);

    QString userConfigPath() const;
    QString configError() const;
    QVariantMap anchors() const;
    QVariantMap modules() const;
    QVariantMap styleOverrides() const;
    QString iconFontFamily() const;
    QString textFontFamily() const;
    QString timeFontFamily() const;
    int bodyFontSize() const;
    int titleFontSize() const;
    int iconFontSize() const;
    int islandHeight() const;
    int islandGap() const;
    int edgeMargin() const;
    int expandedVerticalOffset() const;
    int expandedWidth() const;
    int expandedHeight() const;
    int morphDuration() const;
    int compactFadeDuration() const;
    int contentFadeDuration() const;
    QString compositor() const;

    Q_INVOKABLE QVariantList anchorModules(const QString &anchor) const;
    Q_INVOKABLE QVariant moduleConfig(const QString &moduleId) const;
    Q_INVOKABLE bool moduleEnabled(const QString &moduleId) const;
    Q_INVOKABLE int modulePriority(const QString &moduleId) const;
    Q_INVOKABLE QString moduleCompactMode(const QString &moduleId) const;
    Q_INVOKABLE QString moduleAction(const QString &moduleId, const QString &actionName) const;
    Q_INVOKABLE QString moduleScrollAction(const QString &moduleId, const QString &direction) const;
    Q_INVOKABLE QString styleOverride(const QString &tokenName) const;
    Q_INVOKABLE int mouseButton(const QVariant &button) const;
    Q_INVOKABLE int mouseButtonsMask(const QVariant &buttons) const;
    Q_INVOKABLE void reload();

signals:
    void configChanged();
    void configErrorChanged();

private slots:
    void scheduleReload();

private:
    struct LoadedConfig {
        QVariantMap anchors;
        QVariantMap modules;
        QVariantMap styleOverrides;
        QString iconFontFamily;
        QString textFontFamily;
        QString timeFontFamily;
        int bodyFontSize = 16;
        int titleFontSize = 20;
        int iconFontSize = 18;
        int islandHeight = 38;
        int islandGap = 10;
        int edgeMargin = 16;
        int expandedVerticalOffset = 12;
        int expandedWidth = 520;
        int expandedHeight = 380;
        int morphDuration = 480;
        int compactFadeDuration = 150;
        int contentFadeDuration = 180;
        QString compositor;
        QString error;
    };

    void loadConfig();
    void updateWatchedPaths();
    QString configHome() const;
    LoadedConfig parseConfigObject(const QJsonObject &configObject, const QStringList &initialErrors) const;
    void applyLoadedConfig(const LoadedConfig &next);

    QString m_userConfigPath;
    QString m_configError;
    QVariantMap m_anchors;
    QVariantMap m_modules;
    QVariantMap m_styleOverrides;
    QString m_iconFontFamily;
    QString m_textFontFamily;
    QString m_timeFontFamily;
    int m_bodyFontSize = 16;
    int m_titleFontSize = 20;
    int m_iconFontSize = 18;
    int m_islandHeight = 38;
    int m_islandGap = 10;
    int m_edgeMargin = 16;
    int m_expandedVerticalOffset = 12;
    int m_expandedWidth = 520;
    int m_expandedHeight = 380;
    int m_morphDuration = 480;
    int m_compactFadeDuration = 150;
    int m_contentFadeDuration = 180;
    QString m_compositor;

    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
};
