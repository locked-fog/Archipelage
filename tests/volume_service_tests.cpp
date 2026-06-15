#include "VolumeService.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class VolumeServiceTests : public QObject {
    Q_OBJECT

private slots:
    void constructsWithDefaultState()
    {
        VolumeService service;
        QCOMPARE(service.volume(), 0.0);
        QCOMPARE(service.muted(), false);
        QCOMPARE(service.bluetoothAudio(), false);
    }

    void requestVolumeIsIdempotent()
    {
        VolumeService service;
        service.requestVolume();
        service.requestVolume();
        // No assertion: must not deadlock or crash even when wpctl
        // is not on PATH (the underlying QProcess simply errors out).
    }
};

QTEST_GUILESS_MAIN(VolumeServiceTests)
#include "volume_service_tests.moc"
