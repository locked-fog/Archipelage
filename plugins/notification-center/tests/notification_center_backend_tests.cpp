#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest/QtTest>

#include "NotificationCenterService.h"

class NotificationCenterBackendTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("ARCHIPELAGO_NOTIFICATIONS_DISABLE_DBUS_FOR_TESTS", "1");
        QVERIFY(m_settingsDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    }

    void parsesActionPairs()
    {
        const QVariantList actions = NotificationCenterService::actionEntries({
            QStringLiteral("default"),
            QStringLiteral("Open"),
            QStringLiteral("reply"),
            QStringLiteral("Reply"),
            QStringLiteral("dangling"),
        });

        QCOMPARE(actions.size(), 2);
        QCOMPARE(actions.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("default"));
        QCOMPARE(actions.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Open"));
        QCOMPARE(actions.at(1).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("reply"));
    }

    void notifyStoresUnreadNotification()
    {
        NotificationCenterService service;

        const uint id = service.notify(QStringLiteral("Mail"),
                                       0,
                                       QString(),
                                       QStringLiteral("Inbox"),
                                       QStringLiteral("Quarterly update"),
                                       {QStringLiteral("default"), QStringLiteral("Open")},
                                       {},
                                       -1);

        QCOMPARE(id, 1u);
        QCOMPARE(service.unreadCount(), 1);
        QCOMPARE(service.notifications().size(), 1);

        const QVariantMap notification = service.notifications().constFirst().toMap();
        QCOMPARE(notification.value(QStringLiteral("id")).toUInt(), id);
        QCOMPARE(notification.value(QStringLiteral("appName")).toString(), QStringLiteral("Mail"));
        QCOMPARE(notification.value(QStringLiteral("summary")).toString(), QStringLiteral("Inbox"));
        QCOMPARE(notification.value(QStringLiteral("body")).toString(), QStringLiteral("Quarterly update"));
        QCOMPARE(notification.value(QStringLiteral("unread")).toBool(), true);
        QCOMPARE(notification.value(QStringLiteral("actions")).toList().size(), 1);
    }

    void notifyPreservesUnreadHistoryForReplacesId()
    {
        NotificationCenterService service;

        const uint id = service.notify(QStringLiteral("Chat"), 0, QString(), QStringLiteral("Old"), QString(), {}, {}, -1);
        const uint replaced = service.notify(QStringLiteral("Chat"),
                                             id,
                                             QString(),
                                             QStringLiteral("New"),
                                             QStringLiteral("Updated"),
                                             {},
                                             {},
                                             -1);

        QCOMPARE(replaced, id);
        QCOMPARE(service.notifications().size(), 2);
        QCOMPARE(service.unreadCount(), 2);

        const QVariantMap latest = service.notifications().constFirst().toMap();
        const QVariantMap previous = service.notifications().constLast().toMap();
        QCOMPARE(latest.value(QStringLiteral("id")).toUInt(), 2u);
        QCOMPARE(latest.value(QStringLiteral("sourceId")).toUInt(), id);
        QCOMPARE(latest.value(QStringLiteral("summary")).toString(), QStringLiteral("New"));
        QCOMPARE(latest.value(QStringLiteral("body")).toString(), QStringLiteral("Updated"));
        QCOMPARE(previous.value(QStringLiteral("id")).toUInt(), 1u);
        QCOMPARE(previous.value(QStringLiteral("sourceId")).toUInt(), id);
    }

    void notificationImageSourcesPreferSenderImages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QImage image(2, 2, QImage::Format_RGBA8888);
        image.fill(Qt::red);
        const QString filePath = dir.filePath(QStringLiteral("avatar.png"));
        QVERIFY(image.save(filePath, "PNG"));

        QVariantMap hints;
        hints.insert(QStringLiteral("image-path"), filePath);

        QCOMPARE(NotificationCenterService::notificationImageSource(QStringLiteral("qq"), hints),
                 QUrl::fromLocalFile(filePath).toString());
        QCOMPARE(NotificationCenterService::notificationIconName(QStringLiteral("qq")), QStringLiteral("qq"));
        QCOMPARE(NotificationCenterService::notificationIconName(filePath), QString());
    }

    void notificationImageSourcesCacheInlineImageData()
    {
        QByteArray pixels;
        pixels.append(char(0x1f));
        pixels.append(char(0x80));
        pixels.append(char(0xff));
        pixels.append(char(0xff));

        QVariantMap hints;
        hints.insert(QStringLiteral("image-data"), QVariantList({
                         1,
                         1,
                         4,
                         true,
                         8,
                         4,
                         pixels,
                     }));

        const QString source = NotificationCenterService::notificationImageSource(QString(), hints);
        QVERIFY(source.startsWith(QStringLiteral("file://")));
        QVERIFY(QFileInfo::exists(QUrl(source).toLocalFile()));
    }

    void invokeDefaultActionEmitsActionAndCloses()
    {
        NotificationCenterService service;
        QSignalSpy actionSpy(&service, &NotificationCenterService::actionInvoked);
        QSignalSpy closedSpy(&service, &NotificationCenterService::notificationClosed);

        const uint id = service.notify(QStringLiteral("Calendar"),
                                       0,
                                       QString(),
                                       QStringLiteral("Standup"),
                                       QString(),
                                       {QStringLiteral("default"), QStringLiteral("Open")},
                                       {},
                                       -1);

        QCOMPARE(service.invokeNotification(static_cast<int>(id), QStringLiteral("default")), true);
        QCOMPARE(actionSpy.size(), 1);
        QCOMPARE(actionSpy.at(0).at(0).toUInt(), id);
        QCOMPARE(actionSpy.at(0).at(1).toString(), QStringLiteral("default"));
        QCOMPARE(closedSpy.size(), 1);
        QCOMPARE(closedSpy.at(0).at(0).toUInt(), id);
        QCOMPARE(closedSpy.at(0).at(1).toUInt(), 2u);
        QCOMPARE(service.unreadCount(), 0);
        QCOMPARE(service.notifications().size(), 0);
    }

    void missingDefaultActionMarksReadWithoutSignal()
    {
        NotificationCenterService service;
        QSignalSpy actionSpy(&service, &NotificationCenterService::actionInvoked);
        QSignalSpy closedSpy(&service, &NotificationCenterService::notificationClosed);

        const uint id = service.notify(QStringLiteral("Build"),
                                       0,
                                       QString(),
                                       QStringLiteral("Done"),
                                       QString(),
                                       {QStringLiteral("details"), QStringLiteral("Details")},
                                       {},
                                       -1);

        QCOMPARE(service.invokeNotification(static_cast<int>(id), QStringLiteral("default")), false);
        QCOMPARE(actionSpy.size(), 0);
        QCOMPARE(closedSpy.size(), 0);
        QCOMPARE(service.unreadCount(), 0);
        QCOMPARE(service.notifications().size(), 1);
        QCOMPARE(service.notifications().constFirst().toMap().value(QStringLiteral("unread")).toBool(), false);
    }

    void clearAllClosesAsUserDismissed()
    {
        NotificationCenterService service;
        QSignalSpy closedSpy(&service, &NotificationCenterService::notificationClosed);

        service.notify(QStringLiteral("One"), 0, QString(), QStringLiteral("A"), QString(), {}, {}, -1);
        service.notify(QStringLiteral("Two"), 0, QString(), QStringLiteral("B"), QString(), {}, {}, -1);

        QCOMPARE(service.clearAll(), 2);
        QCOMPARE(closedSpy.size(), 2);
        QCOMPARE(closedSpy.at(0).at(1).toUInt(), 2u);
        QCOMPARE(closedSpy.at(1).at(1).toUInt(), 2u);
        QCOMPARE(service.notifications().size(), 0);
        QCOMPARE(service.unreadCount(), 0);
    }

    void closeUnknownNotificationIsIgnored()
    {
        NotificationCenterService service;
        QSignalSpy closedSpy(&service, &NotificationCenterService::notificationClosed);

        QCOMPARE(service.closeNotificationFromClient(404), false);
        QCOMPARE(closedSpy.size(), 0);
        QCOMPARE(service.notifications().size(), 0);
        QCOMPARE(service.unreadCount(), 0);
    }

    void clientCloseDoesNotRemoveUnreadNotification()
    {
        NotificationCenterService service;
        QSignalSpy closedSpy(&service, &NotificationCenterService::notificationClosed);

        const uint sourceId = service.notify(QStringLiteral("Chat"),
                                             0,
                                             QString(),
                                             QStringLiteral("Ping"),
                                             QString(),
                                             {},
                                             {},
                                             -1);

        QCOMPARE(service.closeNotificationFromClient(sourceId), true);
        QCOMPARE(closedSpy.size(), 1);
        QCOMPARE(closedSpy.at(0).at(0).toUInt(), sourceId);
        QCOMPARE(closedSpy.at(0).at(1).toUInt(), 3u);
        QCOMPARE(service.notifications().size(), 1);
        QCOMPARE(service.unreadCount(), 1);

        QCOMPARE(service.closeNotificationFromClient(sourceId), true);
        QCOMPARE(closedSpy.size(), 1);
    }

    void cyclesModes()
    {
        NotificationCenterService service;

        service.setMode(QStringLiteral("normal"));
        QCOMPARE(service.mode(), QStringLiteral("normal"));
        service.cycleMode();
        QCOMPARE(service.mode(), QStringLiteral("low"));
        service.cycleMode();
        QCOMPARE(service.mode(), QStringLiteral("dnd"));
        service.cycleMode();
        QCOMPARE(service.mode(), QStringLiteral("normal"));

        service.setMode(QStringLiteral("invalid"));
        QCOMPARE(service.mode(), QStringLiteral("normal"));
    }

private:
    QTemporaryDir m_settingsDir;
};

QTEST_MAIN(NotificationCenterBackendTests)
#include "notification_center_backend_tests.moc"
