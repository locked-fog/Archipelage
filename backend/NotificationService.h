#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QtQml/qqml.h>

#include <functional>

// NotificationService mirrors every
// org.freedesktop.Notifications.Notify D-Bus call as a QML signal.
// The watch is implemented by spawning `dbus-monitor` and parsing
// its line-oriented stdout (the same protocol as dbus-monitor
// prints, not the actual D-Bus wire format), then a small state
// machine extracts the (app_name, summary, body) triple from the
// method-call arguments.
//
// Extracted from the legacy SystemServices. The notifications
// plugin's Expanded.qml subscribes to notificationReceived to
// surface incoming desktop notifications.
class NotificationService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit NotificationService(QObject *parent = nullptr);
    ~NotificationService() override;

signals:
    void notificationReceived(const QString &appName,
                               const QString &summary,
                               const QString &body);

private slots:
    void startMonitor();
    void handleOutput();

private:
    void processLines(QByteArray &buffer, const QByteArray &chunk,
                      const std::function<void(const QString &)> &handler);
    void handleLine(const QString &line);
    QString decodeMonitorString(const QString &line) const;

    QProcess *m_monitor = nullptr;
    QTimer m_restartTimer;
    QByteArray m_buffer;
    bool m_captureActive = false;
    int m_captureStage = -1;
    QString m_pendingAppName;
    QString m_pendingSummary;
    QString m_pendingBody;
    bool m_shuttingDown = false;
};
