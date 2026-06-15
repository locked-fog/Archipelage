#include "NotificationService.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class NotificationServiceTests : public QObject {
    Q_OBJECT

private slots:
    void monitorUsesSessionBus()
    {
        const QStringList args = NotificationService::monitorArguments();
        QVERIFY(args.contains(QStringLiteral("--session")));
        QVERIFY(!args.contains(QStringLiteral("--system")));
        QVERIFY(args.contains(QStringLiteral("type='method_call',interface='org.freedesktop.Notifications',member='Notify'")));
    }

    void constructsWithoutDbusMonitor()
    {
        // On a CI host without `dbus-monitor` on PATH the service
        // must construct without crashing; startMonitor() simply
        // logs a warning and returns. The signal is never emitted.
        NotificationService service;
        QSignalSpy spy(&service, &NotificationService::notificationReceived);
        QVERIFY(spy.isEmpty());
    }
};

QTEST_GUILESS_MAIN(NotificationServiceTests)
#include "notification_service_tests.moc"
