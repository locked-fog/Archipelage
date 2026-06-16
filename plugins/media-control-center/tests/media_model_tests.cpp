#include "MprisModel.h"

#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QtTest/QtTest>

class MediaModelTests : public QObject {
    Q_OBJECT

private slots:
    void selectsRequestedPlayerWhenPresent()
    {
        const QList<Mpris::PlayerInfo> players = {
            {QStringLiteral("org.mpris.MediaPlayer2.b"), QStringLiteral("B"), QStringLiteral("Playing")},
            {QStringLiteral("org.mpris.MediaPlayer2.a"), QStringLiteral("A"), QStringLiteral("Paused")},
        };
        QCOMPARE(Mpris::selectPreferredPlayer(players, QStringLiteral("org.mpris.MediaPlayer2.a")),
                 QStringLiteral("org.mpris.MediaPlayer2.a"));
    }

    void prefersPlayingThenPausedThenSortedService()
    {
        const QList<Mpris::PlayerInfo> players = {
            {QStringLiteral("org.mpris.MediaPlayer2.z"), QStringLiteral("Z"), QStringLiteral("Stopped")},
            {QStringLiteral("org.mpris.MediaPlayer2.b"), QStringLiteral("B"), QStringLiteral("Paused")},
            {QStringLiteral("org.mpris.MediaPlayer2.a"), QStringLiteral("A"), QStringLiteral("Playing")},
        };
        QCOMPARE(Mpris::selectPreferredPlayer(players), QStringLiteral("org.mpris.MediaPlayer2.a"));
    }

    void fallsBackToSortedServiceWithinSameStatus()
    {
        const QList<Mpris::PlayerInfo> players = {
            {QStringLiteral("org.mpris.MediaPlayer2.z"), QStringLiteral("Z"), QStringLiteral("Paused")},
            {QStringLiteral("org.mpris.MediaPlayer2.a"), QStringLiteral("A"), QStringLiteral("Paused")},
        };
        QCOMPARE(Mpris::selectPreferredPlayer(players), QStringLiteral("org.mpris.MediaPlayer2.a"));
    }

    void parsesMetadataFields()
    {
        QVariantMap metadata;
        metadata.insert(QStringLiteral("xesam:title"), QVariant::fromValue(QDBusVariant(QStringLiteral("Track"))));
        metadata.insert(QStringLiteral("xesam:artist"),
                        QVariant::fromValue(QDBusVariant(QStringList({QStringLiteral("A"), QStringLiteral("B")}))));
        metadata.insert(QStringLiteral("xesam:album"), QVariant::fromValue(QDBusVariant(QStringLiteral("Album"))));
        metadata.insert(QStringLiteral("mpris:artUrl"), QVariant::fromValue(QDBusVariant(QStringLiteral("file:///tmp/art.png"))));
        metadata.insert(QStringLiteral("mpris:length"), QVariant::fromValue(QDBusVariant(qint64(120000000))));
        metadata.insert(QStringLiteral("mpris:trackid"),
                        QVariant::fromValue(QDBusVariant(QVariant::fromValue(QDBusObjectPath(QStringLiteral("/track/1"))))));

        QCOMPARE(Mpris::titleFromMetadata(metadata), QStringLiteral("Track"));
        QCOMPARE(Mpris::subtitleFromMetadata(metadata), QStringLiteral("A, B"));
        QCOMPARE(Mpris::integerFromMetadata(metadata, QStringLiteral("mpris:length")), qint64(120000000));

        const QVariantMap sanitized = Mpris::sanitizeMetadata(metadata);
        QCOMPARE(sanitized.value(QStringLiteral("artist")).toString(), QStringLiteral("A, B"));
        QCOMPARE(sanitized.value(QStringLiteral("album")).toString(), QStringLiteral("Album"));
        QCOMPARE(sanitized.value(QStringLiteral("artUrl")).toString(), QStringLiteral("file:///tmp/art.png"));
        QCOMPARE(Mpris::trackIdFromMetadata(metadata).path(), QStringLiteral("/track/1"));
    }

    void suppliesStableFallbackTitle()
    {
        const QVariantMap metadata;
        QCOMPARE(Mpris::titleFromMetadata(metadata), QStringLiteral("Unknown title"));
        QCOMPARE(Mpris::integerFromMetadata(metadata, QStringLiteral("mpris:length"), 7), qint64(7));
    }
};

QTEST_GUILESS_MAIN(MediaModelTests)
#include "media_model_tests.moc"
