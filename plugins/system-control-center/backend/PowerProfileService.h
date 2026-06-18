#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <functional>

class PowerProfileService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    explicit PowerProfileService(QObject *parent = nullptr);

    bool available() const;
    QString activeProfile() const;
    QVariantList profiles() const;
    QString lastError() const;

    static QVariantList parseProfiles(const QString &text);
    static QString normalizeProfileName(const QString &text);

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setProfile(const QString &profileId);

signals:
    void stateChanged();
    void profilesChanged();
    void lastErrorChanged();

private:
    struct CommandResult {
        int exitCode = -1;
        QProcess::ExitStatus exitStatus = QProcess::NormalExit;
        QByteArray stdoutData;
        QByteArray stderrData;
        QString errorString;
    };

    using CommandCallback = std::function<void(const CommandResult &)>;

    void start();
    void stop();
    void runCommand(const QString &program, const QStringList &arguments, CommandCallback callback);
    void finishRefresh();
    void setLastError(const QString &message);
    void clearLastError();
    QString commandMessage(const QString &program, const CommandResult &result) const;
    static QString trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData);

    QSet<QString> m_clients;
    QTimer m_refreshTimer;
    bool m_started = false;
    bool m_refreshInFlight = false;
    bool m_refreshQueued = false;
    bool m_available = false;
    QString m_activeProfile;
    QVariantList m_profiles;
    QString m_lastError;
};
