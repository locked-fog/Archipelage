#pragma once

#include <QByteArray>
#include <QVariantList>
#include <QVector>

namespace Cava {

struct Settings {
    int barCount = 6;
    int asciiMaxRange = 1000;
    int frameRate = 48;
    int sensitivity = 100;
};

QByteArray buildRawConfig(const Settings &settings);
QVector<qreal> zeroLevels(int barCount);
bool parseAsciiFrame(const QByteArray &frame, int expectedBars, int asciiMaxRange, QVector<qreal> *levelsOut);
QVariantList toVariantList(const QVector<qreal> &levels);

}  // namespace Cava
