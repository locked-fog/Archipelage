#include "CavaSupport.h"

#include <QStringList>

namespace Cava {

QByteArray buildRawConfig(const Settings &settings)
{
    const int safeBarCount = settings.barCount > 0 ? settings.barCount : 6;
    const int safeAsciiMaxRange = settings.asciiMaxRange > 0 ? settings.asciiMaxRange : 1000;
    const int safeFrameRate = settings.frameRate > 0 ? settings.frameRate : 48;
    const int safeSensitivity = settings.sensitivity > 0 ? settings.sensitivity : 100;

    const QStringList lines = {
        QStringLiteral("[general]"),
        QStringLiteral("bars = %1").arg(safeBarCount),
        QStringLiteral("framerate = %1").arg(safeFrameRate),
        QStringLiteral("sensitivity = %1").arg(safeSensitivity),
        QStringLiteral("autosens = 1"),
        QStringLiteral("sleep_timer = 0"),
        QString(),
        QStringLiteral("[output]"),
        QStringLiteral("method = raw"),
        QStringLiteral("channels = mono"),
        QStringLiteral("mono_option = average"),
        QStringLiteral("raw_target = /dev/stdout"),
        QStringLiteral("data_format = ascii"),
        QStringLiteral("ascii_max_range = %1").arg(safeAsciiMaxRange),
        QStringLiteral("bar_delimiter = 59"),
        QStringLiteral("frame_delimiter = 10"),
        QString(),
    };
    return lines.join(QLatin1Char('\n')).toUtf8();
}

QVector<qreal> zeroLevels(int barCount)
{
    const int safeBarCount = barCount > 0 ? barCount : 6;
    return QVector<qreal>(safeBarCount, 0.0);
}

bool parseAsciiFrame(const QByteArray &frame, int expectedBars, int asciiMaxRange, QVector<qreal> *levelsOut)
{
    if (!levelsOut || expectedBars <= 0 || asciiMaxRange <= 0)
        return false;

    const QList<QByteArray> parts = frame.trimmed().split(';');
    if (parts.size() != expectedBars)
        return false;

    QVector<qreal> parsed;
    parsed.reserve(parts.size());
    for (const QByteArray &part : parts) {
        bool ok = false;
        const int rawValue = part.trimmed().toInt(&ok);
        if (!ok)
            return false;

        const int clamped = rawValue < 0 ? 0 : (rawValue > asciiMaxRange ? asciiMaxRange : rawValue);
        parsed.append(static_cast<qreal>(clamped) / static_cast<qreal>(asciiMaxRange));
    }

    *levelsOut = parsed;
    return true;
}

QVariantList toVariantList(const QVector<qreal> &levels)
{
    QVariantList result;
    result.reserve(levels.size());
    for (qreal level : levels)
        result.append(level);
    return result;
}

}  // namespace Cava
