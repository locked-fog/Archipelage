#include "BrightnessService.h"

#include <QtTest/QtTest>

class BrightnessServiceTests : public QObject {
    Q_OBJECT

private slots:
    void constructsWithDefaultState()
    {
        // On hosts without /sys/class/backlight entries brightness
        // stays at 0. On hosts with backlight it reflects the sysfs
        // value normalised by max_brightness (so 0..1).
        BrightnessService service;
        QVERIFY(service.brightness() >= 0.0);
        QVERIFY(service.brightness() <= 1.0);
    }

    void requestBrightnessIsIdempotent()
    {
        BrightnessService service;
        service.requestBrightness();
        service.requestBrightness();
        // No assertion; must not crash.
    }
};

QTEST_GUILESS_MAIN(BrightnessServiceTests)
#include "brightness_service_tests.moc"
