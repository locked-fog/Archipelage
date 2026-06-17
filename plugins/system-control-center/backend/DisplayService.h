#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <functional>

class DisplayService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString activeDeviceId READ activeDeviceId NOTIFY stateChanged FINAL)
    Q_PROPERTY(double brightness READ brightness NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    explicit DisplayService(QObject *parent = nullptr);

    bool available() const;
    QString activeDeviceId() const;
    double brightness() const;
    QVariantList devices() const;
    QString lastError() const;

    static QStringList parseDeviceList(const QString &text);
    static QVariantMap parseDeviceInfo(const QString &deviceId, const QString &text);

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setActiveDevice(const QString &deviceId);
    Q_INVOKABLE void setBrightness(double value);
    Q_INVOKABLE void setDeviceBrightness(const QString &deviceId, double value);

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
    void refreshDeviceDetails(const QStringList &deviceIds, int index, const QVariantList &currentDevices);
    void finishRefresh();
    void syncActiveDevice(const QVariantList &devices);
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
    QString m_activeDeviceId;
    double m_brightness = 0.0;
    QVariantList m_devices;
    QString m_lastError;
};
