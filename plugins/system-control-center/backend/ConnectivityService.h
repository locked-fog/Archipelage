#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <functional>

class ConnectivityService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool wiredConnected READ wiredConnected NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString activeSsid READ activeSsid NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString wiredConnectionName READ wiredConnectionName NOTIFY stateChanged FINAL)
    Q_PROPERTY(int activeSignal READ activeSignal NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    struct WifiDeviceState {
        QString deviceName;
        QString stateText;
        QString connectionName;
        bool connected = false;
        bool wiredConnected = false;
        QString wiredConnectionName;
    };

    explicit ConnectivityService(QObject *parent = nullptr);

    bool available() const;
    bool wifiEnabled() const;
    bool wifiConnected() const;
    bool wiredConnected() const;
    QString activeSsid() const;
    QString wiredConnectionName() const;
    int activeSignal() const;
    QString statusText() const;
    QVariantList networks() const;
    QString lastError() const;

    static bool parseWifiEnabled(const QString &text);
    static QVariantList parseWifiList(const QString &text);
    static WifiDeviceState parseWifiDeviceState(const QString &text);
    static QVariantList parseSavedConnections(const QString &text);

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void requestScan();
    Q_INVOKABLE void setWifiEnabled(bool enabled);
    Q_INVOKABLE void connectToNetwork(const QString &ssid);
    Q_INVOKABLE void disconnectActive();
    Q_INVOKABLE void setAutoConnect(const QString &uuid, bool enabled);
    Q_INVOKABLE void forgetNetwork(const QString &uuid);

signals:
    void stateChanged();
    void networksChanged();
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
    static QStringList splitEscapedFields(const QString &line);

    QSet<QString> m_clients;
    QTimer m_refreshTimer;
    bool m_started = false;
    bool m_refreshInFlight = false;
    bool m_refreshQueued = false;
    bool m_available = false;
    bool m_wifiEnabled = false;
    bool m_wifiConnected = false;
    bool m_wiredConnected = false;
    QString m_activeSsid;
    QString m_wiredConnectionName;
    int m_activeSignal = 0;
    QString m_statusText = QStringLiteral("Wi-Fi unavailable");
    QString m_wifiDeviceName;
    QVariantList m_networks;
    QString m_lastError;
};
