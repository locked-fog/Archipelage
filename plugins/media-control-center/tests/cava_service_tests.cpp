#include "CavaSupport.h"

#include <QtTest/QtTest>

class CavaServiceTests : public QObject {
    Q_OBJECT

private slots:
    void buildsRawConfigForAsciiStdout()
    {
        Cava::Settings settings;
        settings.barCount = 8;
        settings.asciiMaxRange = 2048;
        settings.frameRate = 60;
        settings.sensitivity = 140;

        const QByteArray config = Cava::buildRawConfig(settings);
        QVERIFY(config.contains("bars = 8"));
        QVERIFY(config.contains("framerate = 60"));
        QVERIFY(config.contains("method = raw"));
        QVERIFY(config.contains("channels = mono"));
        QVERIFY(config.contains("raw_target = /dev/stdout"));
        QVERIFY(config.contains("data_format = ascii"));
        QVERIFY(config.contains("ascii_max_range = 2048"));
    }

    void parsesAsciiFramesIntoNormalizedLevels()
    {
        QVector<qreal> levels;
        QVERIFY(Cava::parseAsciiFrame("0;250;1000;125\n", 4, 1000, &levels));
        QCOMPARE(levels.size(), 4);
        QCOMPARE(levels.at(0), 0.0);
        QCOMPARE(levels.at(1), 0.25);
        QCOMPARE(levels.at(2), 1.0);
        QCOMPARE(levels.at(3), 0.125);
    }

    void rejectsMalformedAsciiFrames()
    {
        QVector<qreal> levels;
        QVERIFY(!Cava::parseAsciiFrame("1;2;3\n", 4, 1000, &levels));
        QVERIFY(!Cava::parseAsciiFrame("1;two;3;4\n", 4, 1000, &levels));
    }
};

QTEST_GUILESS_MAIN(CavaServiceTests)
#include "cava_service_tests.moc"
