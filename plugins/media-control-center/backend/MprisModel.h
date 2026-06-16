#pragma once

#include <QDBusObjectPath>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Mpris {

struct PlayerInfo {
    QString service;
    QString identity;
    QString playbackStatus;
};

QString selectPreferredPlayer(const QList<PlayerInfo> &players, const QString &requestedService = QString());
QStringList artistsFromMetadata(const QVariantMap &metadata);
QString stringFromMetadata(const QVariantMap &metadata, const QString &key);
qint64 integerFromMetadata(const QVariantMap &metadata, const QString &key, qint64 fallback = 0);
QString titleFromMetadata(const QVariantMap &metadata);
QString subtitleFromMetadata(const QVariantMap &metadata);
QVariantMap sanitizeMetadata(const QVariantMap &metadata);
bool playbackStatusIsPlaying(const QString &status);
QDBusObjectPath trackIdFromMetadata(const QVariantMap &metadata);

}  // namespace Mpris
