#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <functional>

class BluetoothService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool powered READ powered NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool discovering READ discovering NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    struct ControllerState {
        bool available = false;
        bool powered = false;
        bool discovering = false;
        QString alias;
    };

    explicit BluetoothService(QObject *parent = nullptr);

    bool available() const;
    bool powered() const;
    bool discovering() const;
    QString connectedDeviceName() const;
    QString statusText() const;
    QVariantList devices() const;
    QString lastError() const;

    static ControllerState parseControllerState(const QString &text);
    static QVariantMap parseDeviceInfo(const QString &address, const QString &text);

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setPowered(bool enabled);
    Q_INVOKABLE void requestScan();
    Q_INVOKABLE void connectDevice(const QString &address);
    Q_INVOKABLE void disconnectDevice(const QString &address);

signals:
    void stateChanged();
    void devicesChanged();
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
    void refreshDeviceDetails(const QStringList &addresses,
                              int index,
                              const QVariantList &currentDevices,
                              const ControllerState &controllerState);
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
    bool m_powered = false;
    bool m_discovering = false;
    QString m_connectedDeviceName;
    QString m_statusText = QStringLiteral("Bluetooth unavailable");
    QVariantList m_devices;
    QString m_lastError;
};
