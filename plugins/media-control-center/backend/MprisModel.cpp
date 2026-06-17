#include "MprisModel.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QVariant>

namespace {

int statusRank(const QString &status)
{
    if (status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0)
        return 3;
    if (status.compare(QStringLiteral("Paused"), Qt::CaseInsensitive) == 0)
        return 2;
    return 1;
}

QStringList variantToStringList(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return variantToStringList(value.value<QDBusVariant>().variant());

    if (value.canConvert<QStringList>())
        return value.toStringList();

    if (value.canConvert<QDBusArgument>())
        return qdbus_cast<QStringList>(value.value<QDBusArgument>());

    QStringList result;
    const QVariantList values = value.toList();
    for (const QVariant &entry : values) {
        const QString text = entry.toString().trimmed();
        if (!text.isEmpty())
            result.append(text);
    }
    return result;
}

}  // namespace

namespace Mpris {

QString selectPreferredPlayer(const QList<PlayerInfo> &players, const QString &requestedService)
{
    if (!requestedService.trimmed().isEmpty()) {
        for (const PlayerInfo &player : players) {
            if (player.service == requestedService)
                return player.service;
        }
    }

    QString selected;
    int selectedRank = 0;
    for (const PlayerInfo &player : players) {
        if (player.service.trimmed().isEmpty())
            continue;

        const int rank = statusRank(player.playbackStatus);
        if (selected.isEmpty() || rank > selectedRank
            || (rank == selectedRank && player.service < selected)) {
            selected = player.service;
            selectedRank = rank;
        }
    }
    return selected;
}

QStringList artistsFromMetadata(const QVariantMap &metadata)
{
    return variantToStringList(metadata.value(QStringLiteral("xesam:artist")));
}

QString stringFromMetadata(const QVariantMap &metadata, const QString &key)
{
    QVariant value = metadata.value(key);
    if (value.canConvert<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    if (value.canConvert<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path();
    return value.toString().trimmed();
}

qint64 integerFromMetadata(const QVariantMap &metadata, const QString &key, qint64 fallback)
{
    QVariant raw = metadata.value(key);
    if (raw.canConvert<QDBusVariant>())
        raw = raw.value<QDBusVariant>().variant();

    bool ok = false;
    const qint64 value = raw.toLongLong(&ok);
    return ok ? value : fallback;
}

QString titleFromMetadata(const QVariantMap &metadata)
{
    const QString title = stringFromMetadata(metadata, QStringLiteral("xesam:title"));
    return title.isEmpty() ? QStringLiteral("Unknown title") : title;
}

QString subtitleFromMetadata(const QVariantMap &metadata)
{
    const QStringList artists = artistsFromMetadata(metadata);
    if (!artists.isEmpty())
        return artists.join(QStringLiteral(", "));
    return stringFromMetadata(metadata, QStringLiteral("xesam:album"));
}

QVariantMap sanitizeMetadata(const QVariantMap &metadata)
{
    QVariantMap result;
    result.insert(QStringLiteral("title"), titleFromMetadata(metadata));
    result.insert(QStringLiteral("artist"), artistsFromMetadata(metadata).join(QStringLiteral(", ")));
    result.insert(QStringLiteral("album"), stringFromMetadata(metadata, QStringLiteral("xesam:album")));
    result.insert(QStringLiteral("artUrl"), stringFromMetadata(metadata, QStringLiteral("mpris:artUrl")));
    result.insert(QStringLiteral("length"), integerFromMetadata(metadata, QStringLiteral("mpris:length")));
    return result;
}

bool playbackStatusIsPlaying(const QString &status)
{
    return status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0;
}

QDBusObjectPath trackIdFromMetadata(const QVariantMap &metadata)
{
    QVariant value = metadata.value(QStringLiteral("mpris:trackid"));
    if (value.canConvert<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    if (value.canConvert<QDBusObjectPath>())
        return value.value<QDBusObjectPath>();

    const QString path = value.toString().trimmed();
    if (!path.isEmpty())
        return QDBusObjectPath(path);
    return QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2"));
}

}  // namespace Mpris
