#include "BatteryService.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class BatteryServiceTests : public QObject {
    Q_OBJECT

private slots:
    void constructsWithDefaultState()
    {
        // On a CI / sandbox without /sys/class/power_supply entries
        // the service must construct without crashing and expose a
        // valid default state. The numeric capacity depends on the
        // host hardware (0 on hosts without a battery, otherwise a
        // reading in 0..100), so we only assert the value is in range.
        BatteryService service;
        QVERIFY(service.capacity() >= 0);
        QVERIFY(service.capacity() <= 100);
        QVERIFY(!service.status().isEmpty());
    }

    void refreshDoesNotCrash()
    {
        BatteryService service;
        service.refresh();
        // No assertion; the call must be a no-op or succeed silently
        // regardless of whether the host has a battery.
    }

    void upowerStateToStatusMapsKnownStates()
    {
        // Static helper. Values 1 and 5 = Charging, 2 and 6 = Discharging.
        QCOMPARE(BatteryService::upowerStateToStatus(1), QStringLiteral("Charging"));
        QCOMPARE(BatteryService::upowerStateToStatus(5), QStringLiteral("Charging"));
        QCOMPARE(BatteryService::upowerStateToStatus(2), QStringLiteral("Discharging"));
        QCOMPARE(BatteryService::upowerStateToStatus(6), QStringLiteral("Discharging"));
        QCOMPARE(BatteryService::upowerStateToStatus(3), QStringLiteral("Empty"));
        QCOMPARE(BatteryService::upowerStateToStatus(4), QStringLiteral("Full"));
        QCOMPARE(BatteryService::upowerStateToStatus(99), QStringLiteral("Unknown"));
    }
};

QTEST_GUILESS_MAIN(BatteryServiceTests)
#include "battery_service_tests.moc"
