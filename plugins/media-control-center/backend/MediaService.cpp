#include "MediaService.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QSignalBlocker>

#include <algorithm>

namespace {

constexpr const char *kObjectPath = "/org/mpris/MediaPlayer2";
constexpr const char *kRootInterface = "org.mpris.MediaPlayer2";
constexpr const char *kPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr const char *kPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr const char *kBusInterface = "org.freedesktop.DBus";
constexpr const char *kBusService = "org.freedesktop.DBus";
constexpr const char *kBusPath = "/org/freedesktop/DBus";
constexpr const char *kMprisPrefix = "org.mpris.MediaPlayer2.";

bool envDisablesDbus()
{
    return qEnvironmentVariableIsSet("ARCHIPELAGO_MEDIA_DISABLE_DBUS_FOR_TESTS");
}

qint64 variantToLongLong(const QVariant &value, qint64 fallback = 0)
{
    bool ok = false;
    const qint64 result = value.toLongLong(&ok);
    return ok ? result : fallback;
}

double variantToDouble(const QVariant &value, double fallback)
{
    bool ok = false;
    const double result = value.toDouble(&ok);
    return ok ? result : fallback;
}

bool variantToBool(const QVariant &value, bool fallback = false)
{
    if (!value.isValid())
        return fallback;
    return value.toBool();
}

QVariant unwrapDbusValue(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return unwrapDbusValue(value.value<QDBusVariant>().variant());
    return value;
}

QVariantMap variantToMap(const QVariant &value)
{
    const QVariant unwrapped = unwrapDbusValue(value);
    if (unwrapped.canConvert<QVariantMap>())
        return unwrapped.toMap();
    if (unwrapped.canConvert<QDBusArgument>())
        return qdbus_cast<QVariantMap>(unwrapped.value<QDBusArgument>());
    return {};
}

}  // namespace

MediaService::MediaService(QObject *parent)
    : QObject(parent)
{
    m_positionTimer.setInterval(1000);
    m_positionTimer.setSingleShot(false);
    connect(&m_positionTimer, &QTimer::timeout, this, &MediaService::refreshPosition);
}

bool MediaService::available() const { return !m_state.service.isEmpty(); }
QVariantList MediaService::players() const { return m_players; }
QString MediaService::activeService() const { return m_state.service; }
QString MediaService::identity() const { return m_state.identity; }
QString MediaService::playbackStatus() const { return m_state.playbackStatus; }
bool MediaService::playing() const { return Mpris::playbackStatusIsPlaying(m_state.playbackStatus); }
QString MediaService::title() const { return Mpris::titleFromMetadata(m_state.metadata); }
QString MediaService::artist() const { return Mpris::subtitleFromMetadata(m_state.metadata); }
QString MediaService::album() const { return Mpris::stringFromMetadata(m_state.metadata, QStringLiteral("xesam:album")); }
QString MediaService::artUrl() const { return Mpris::stringFromMetadata(m_state.metadata, QStringLiteral("mpris:artUrl")); }
qint64 MediaService::duration() const { return m_state.duration; }
qint64 MediaService::position() const { return m_state.position; }
double MediaService::volume() const { return m_state.volume; }
double MediaService::rate() const { return m_state.rate; }
QString MediaService::loopStatus() const { return m_state.loopStatus; }
bool MediaService::shuffle() const { return m_state.shuffle; }
bool MediaService::canGoNext() const { return m_state.canGoNext; }
bool MediaService::canGoPrevious() const { return m_state.canGoPrevious; }
bool MediaService::canPlay() const { return m_state.canPlay; }
bool MediaService::canPause() const { return m_state.canPause; }
bool MediaService::canSeek() const { return m_state.canSeek; }
bool MediaService::canControl() const { return m_state.canControl; }
QString MediaService::lastError() const { return m_lastError; }

void MediaService::registerClient(const QString &clientId)
{
    const QString id = clientId.trimmed().isEmpty() ? QStringLiteral("anonymous") : clientId.trimmed();
    const bool wasEmpty = m_clients.isEmpty();
    m_clients.insert(id);
    if (wasEmpty)
        start();
}

void MediaService::releaseClient(const QString &clientId)
{
    m_clients.remove(clientId.trimmed().isEmpty() ? QStringLiteral("anonymous") : clientId.trimmed());
    if (m_clients.isEmpty())
        stopService();
}

void MediaService::start()
{
    if (m_started)
        return;
    m_started = true;
    connectSignals();
    refresh();
    m_positionTimer.start();
}

void MediaService::stopService()
{
    if (!m_started)
        return;
    m_started = false;
    m_positionTimer.stop();
    disconnectSignals();
}

void MediaService::connectSignals()
{
    if (m_signalsConnected || envDisablesDbus())
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    const bool propertiesConnected = bus.connect(QString(),
                                                 QString::fromLatin1(kObjectPath),
                                                 QString::fromLatin1(kPropertiesInterface),
                                                 QStringLiteral("PropertiesChanged"),
                                                 this,
                                                 SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    const bool namesConnected = bus.connect(QString::fromLatin1(kBusService),
                                            QString::fromLatin1(kBusPath),
                                            QString::fromLatin1(kBusInterface),
                                            QStringLiteral("NameOwnerChanged"),
                                            this,
                                            SLOT(onNameOwnerChanged(QString,QString,QString)));
    m_signalsConnected = propertiesConnected && namesConnected;
}

void MediaService::disconnectSignals()
{
    if (!m_signalsConnected)
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(QString(),
                   QString::fromLatin1(kObjectPath),
                   QString::fromLatin1(kPropertiesInterface),
                   QStringLiteral("PropertiesChanged"),
                   this,
                   SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.disconnect(QString::fromLatin1(kBusService),
                   QString::fromLatin1(kBusPath),
                   QString::fromLatin1(kBusInterface),
                   QStringLiteral("NameOwnerChanged"),
                   this,
                   SLOT(onNameOwnerChanged(QString,QString,QString)));
    m_signalsConnected = false;
}

void MediaService::refresh()
{
    if (envDisablesDbus()) {
        setActiveState(PlayerState());
        return;
    }

    const QStringList services = discoverServices();
    QList<PlayerState> states;
    states.reserve(services.size());
    for (const QString &service : services)
        states.append(readPlayer(service));

    const QVariantList nextPlayers = playerEntries(states);
    if (m_players != nextPlayers) {
        m_players = nextPlayers;
        emit playersChanged();
    }

    const QString selected = Mpris::selectPreferredPlayer(playerInfoList(states), m_requestedService);
    PlayerState nextState;
    for (const PlayerState &state : states) {
        if (state.service == selected) {
            nextState = state;
            break;
        }
    }
    setActiveState(nextState);
}

void MediaService::choosePlayer(const QString &service)
{
    m_requestedService = service.trimmed();
    refresh();
}

void MediaService::playPause() { callPlayerMethod(QStringLiteral("PlayPause")); }
void MediaService::play() { callPlayerMethod(QStringLiteral("Play")); }
void MediaService::pause() { callPlayerMethod(QStringLiteral("Pause")); }
void MediaService::stop() { callPlayerMethod(QStringLiteral("Stop")); }
void MediaService::next() { callPlayerMethod(QStringLiteral("Next")); }
void MediaService::previous() { callPlayerMethod(QStringLiteral("Previous")); }

void MediaService::seek(qint64 offsetUs)
{
    callPlayerMethod(QStringLiteral("Seek"), {offsetUs});
}

void MediaService::setPosition(qint64 positionUs)
{
    if (m_state.service.isEmpty())
        return;
    callPlayerMethod(QStringLiteral("SetPosition"),
                     {QVariant::fromValue(Mpris::trackIdFromMetadata(m_state.metadata)), positionUs});
}

void MediaService::setVolume(double value)
{
    const double clamped = std::max(0.0, std::min(1.0, value));
    if (writePlayerProperty(QStringLiteral("Volume"), clamped))
        refresh();
}

void MediaService::setLoopStatus(const QString &status)
{
    const QString normalized = status == QStringLiteral("Track") || status == QStringLiteral("Playlist")
        ? status
        : QStringLiteral("None");
    if (writePlayerProperty(QStringLiteral("LoopStatus"), normalized))
        refresh();
}

void MediaService::toggleLoopStatus()
{
    if (m_state.loopStatus == QStringLiteral("None") || m_state.loopStatus.isEmpty()) {
        setLoopStatus(QStringLiteral("Playlist"));
    } else if (m_state.loopStatus == QStringLiteral("Playlist")) {
        setLoopStatus(QStringLiteral("Track"));
    } else {
        setLoopStatus(QStringLiteral("None"));
    }
}

void MediaService::setShuffle(bool enabled)
{
    if (writePlayerProperty(QStringLiteral("Shuffle"), enabled))
        refresh();
}

void MediaService::toggleShuffle()
{
    setShuffle(!m_state.shuffle);
}

QStringList MediaService::discoverServices()
{
    QDBusInterface bus(QString::fromLatin1(kBusService),
                       QString::fromLatin1(kBusPath),
                       QString::fromLatin1(kBusInterface),
                       QDBusConnection::sessionBus());
    const QDBusReply<QStringList> reply = bus.call(QStringLiteral("ListNames"));
    if (!reply.isValid()) {
        setLastError(reply.error().message());
        return {};
    }

    QStringList result;
    for (const QString &name : reply.value()) {
        if (name.startsWith(QString::fromLatin1(kMprisPrefix)))
            result.append(name);
    }
    result.sort();
    clearLastError();
    return result;
}

MediaService::PlayerState MediaService::readPlayer(const QString &service)
{
    PlayerState state;
    state.service = service;
    state.identity = readProperty(service, QString::fromLatin1(kRootInterface), QStringLiteral("Identity")).toString();
    state.playbackStatus = readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("PlaybackStatus")).toString();
    state.metadata = variantToMap(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("Metadata")));
    state.duration = Mpris::integerFromMetadata(state.metadata, QStringLiteral("mpris:length"));
    state.position = variantToLongLong(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("Position")));
    state.volume = variantToDouble(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("Volume")), 1.0);
    state.rate = variantToDouble(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("Rate")), 1.0);
    state.loopStatus = readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("LoopStatus")).toString();
    state.shuffle = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("Shuffle")));
    state.canGoNext = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("CanGoNext")));
    state.canGoPrevious = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("CanGoPrevious")));
    state.canPlay = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("CanPlay")));
    state.canPause = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("CanPause")));
    state.canSeek = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("CanSeek")));
    state.canControl = variantToBool(readProperty(service, QString::fromLatin1(kPlayerInterface), QStringLiteral("CanControl")));
    return state;
}

QVariant MediaService::readProperty(const QString &service, const QString &interfaceName, const QString &propertyName)
{
    QDBusInterface properties(service,
                              QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kPropertiesInterface),
                              QDBusConnection::sessionBus());
    const QDBusReply<QDBusVariant> reply = properties.call(QStringLiteral("Get"), interfaceName, propertyName);
    if (!reply.isValid())
        return {};
    return reply.value().variant();
}

bool MediaService::callPlayerMethod(const QString &method, const QVariantList &arguments)
{
    if (m_state.service.isEmpty())
        return false;

    QDBusInterface player(m_state.service,
                          QString::fromLatin1(kObjectPath),
                          QString::fromLatin1(kPlayerInterface),
                          QDBusConnection::sessionBus());
    const QDBusMessage reply = player.callWithArgumentList(QDBus::Block, method, arguments);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        setLastError(reply.errorMessage());
        return false;
    }
    clearLastError();
    refresh();
    return true;
}

bool MediaService::writePlayerProperty(const QString &propertyName, const QVariant &value)
{
    if (m_state.service.isEmpty())
        return false;

    QDBusInterface properties(m_state.service,
                              QString::fromLatin1(kObjectPath),
                              QString::fromLatin1(kPropertiesInterface),
                              QDBusConnection::sessionBus());
    const QDBusMessage reply = properties.call(QStringLiteral("Set"),
                                               QString::fromLatin1(kPlayerInterface),
                                               propertyName,
                                               QVariant::fromValue(QDBusVariant(value)));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        setLastError(reply.errorMessage());
        return false;
    }
    clearLastError();
    return true;
}

void MediaService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void MediaService::clearLastError()
{
    setLastError(QString());
}

void MediaService::setActiveState(const PlayerState &state)
{
    const bool samePosition = m_state.position == state.position;
    m_state = state;
    emit stateChanged();
    if (!samePosition)
        emit positionChanged();
}

QVariantList MediaService::playerEntries(const QList<PlayerState> &states) const
{
    QVariantList result;
    for (const PlayerState &state : states) {
        QVariantMap entry;
        entry.insert(QStringLiteral("service"), state.service);
        entry.insert(QStringLiteral("identity"), state.identity.isEmpty() ? state.service : state.identity);
        entry.insert(QStringLiteral("playbackStatus"), state.playbackStatus);
        result.append(entry);
    }
    return result;
}

QList<Mpris::PlayerInfo> MediaService::playerInfoList(const QList<PlayerState> &states) const
{
    QList<Mpris::PlayerInfo> result;
    result.reserve(states.size());
    for (const PlayerState &state : states)
        result.append({state.service, state.identity, state.playbackStatus});
    return result;
}

void MediaService::onPropertiesChanged(const QString &interfaceName,
                                       const QVariantMap &changedProperties,
                                       const QStringList &invalidatedProperties)
{
    Q_UNUSED(changedProperties)
    Q_UNUSED(invalidatedProperties)
    if (interfaceName == QString::fromLatin1(kPlayerInterface)
        || interfaceName == QString::fromLatin1(kRootInterface)) {
        refresh();
    }
}

void MediaService::onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    if (name.startsWith(QString::fromLatin1(kMprisPrefix)))
        refresh();
}

void MediaService::refreshPosition()
{
    if (m_state.service.isEmpty())
        return;

    const qint64 nextPosition = variantToLongLong(readProperty(m_state.service,
                                                               QString::fromLatin1(kPlayerInterface),
                                                               QStringLiteral("Position")),
                                                  m_state.position);
    if (m_state.position == nextPosition)
        return;
    m_state.position = nextPosition;
    emit positionChanged();
}
