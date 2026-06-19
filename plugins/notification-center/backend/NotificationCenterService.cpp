#include "NotificationCenterService.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QSettings>

#include <algorithm>

namespace {
Q_LOGGING_CATEGORY(lcNotifications, "archipelago.plugins.notifications")

constexpr const char *kServiceName = "org.freedesktop.Notifications";
constexpr const char *kObjectPath = "/org/freedesktop/Notifications";
constexpr const char *kSettingsOrg = "Archipelago";
constexpr const char *kSettingsApp = "NotificationCenter";

bool envDisablesDbus()
{
    return qEnvironmentVariableIsSet("ARCHIPELAGO_NOTIFICATIONS_DISABLE_DBUS_FOR_TESTS");
}

QVariant unwrapHint(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}
}  // namespace

class NotificationServerAdaptor final : public QDBusAbstractAdaptor, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationServerAdaptor(NotificationCenterService *service)
        : QDBusAbstractAdaptor(service)
        , m_service(service)
    {
        setAutoRelaySignals(true);
    }

public slots:
    QStringList GetCapabilities()
    {
        return {
            QStringLiteral("actions"),
            QStringLiteral("body"),
            QStringLiteral("persistence"),
        };
    }

    uint Notify(const QString &app_name,
                uint replaces_id,
                const QString &app_icon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int expire_timeout)
    {
        return m_service->notify(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout);
    }

    void CloseNotification(uint id)
    {
        if (!m_service->closeNotificationFromClient(id))
            sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Unknown notification id"));
    }

    void GetServerInformation(QString &name, QString &vendor, QString &version, QString &spec_version)
    {
        name = QStringLiteral("Archipelago Notification Center");
        vendor = QStringLiteral("Archipelago");
        version = QStringLiteral("0.1.0");
        spec_version = QStringLiteral("1.3");
    }

signals:
    void ActionInvoked(uint id, const QString &action_key);
    void NotificationClosed(uint id, uint reason);

private:
    NotificationCenterService *m_service = nullptr;
};

NotificationCenterService::NotificationCenterService(QObject *parent)
    : QObject(parent)
{
    loadMode();
}

NotificationCenterService::~NotificationCenterService()
{
    stopServer();
}

bool NotificationCenterService::serverAvailable() const { return m_serverAvailable; }
QString NotificationCenterService::mode() const { return m_mode; }
QVariantList NotificationCenterService::notifications() const
{
    QVariantList result;
    result.reserve(m_records.size());
    for (const NotificationRecord &record : m_records)
        result.append(toMap(record));
    return result;
}

QVariantMap NotificationCenterService::latestNotification() const
{
    return m_records.isEmpty() ? QVariantMap() : toMap(m_records.constFirst());
}

int NotificationCenterService::unreadCount() const { return calculateUnreadCount(); }
int NotificationCenterService::maxNotifications() const { return m_maxNotifications; }
QString NotificationCenterService::lastError() const { return m_lastError; }

QVariantList NotificationCenterService::actionEntries(const QStringList &actions)
{
    QVariantList result;
    for (int index = 0; index + 1 < actions.size(); index += 2) {
        const QString key = actions.at(index).trimmed();
        const QString label = actions.at(index + 1).trimmed();
        if (key.isEmpty())
            continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("key"), key);
        entry.insert(QStringLiteral("label"), label.isEmpty() ? key : label);
        result.append(entry);
    }
    return result;
}

QString NotificationCenterService::normalizedMode(const QString &mode)
{
    const QString value = mode.trimmed().toLower();
    if (value == QStringLiteral("normal") || value == QStringLiteral("low") || value == QStringLiteral("dnd"))
        return value;
    return QString();
}

uint NotificationCenterService::notify(const QString &appName,
                                       uint replacesId,
                                       const QString &appIcon,
                                       const QString &summary,
                                       const QString &body,
                                       const QStringList &actions,
                                       const QVariantMap &hints,
                                       int expireTimeout)
{
    const int previousUnread = calculateUnreadCount();
    const int replaceIndex = replacesId > 0 ? indexForId(replacesId) : -1;
    NotificationRecord record;
    record.id = replaceIndex >= 0 ? replacesId : nextNotificationId();
    record.appName = appName;
    record.appIcon = appIcon;
    record.summary = summary;
    record.body = body;
    record.actions = actionEntries(actions);
    record.hints = hints;
    record.createdAt = QDateTime::currentDateTimeUtc();
    record.expireTimeout = expireTimeout;
    record.unread = true;
    record.resident = hintBool(hints, QStringLiteral("resident"));
    record.transient = hintBool(hints, QStringLiteral("transient"));
    record.urgency = hintInt(hints, QStringLiteral("urgency"), 1);

    if (replaceIndex >= 0) {
        m_records[replaceIndex] = record;
        m_records.move(replaceIndex, 0);
    } else {
        m_records.prepend(record);
    }

    enforceMaxNotifications();
    const QVariantMap payload = toMap(record);
    emitCollectionChanged(previousUnread);
    emit notificationArrived(payload);
    return record.id;
}

bool NotificationCenterService::closeNotificationFromClient(uint id)
{
    const int index = indexForId(id);
    if (index < 0)
        return false;
    return closeNotificationAt(index, ClosedByCall);
}

void NotificationCenterService::registerClient(const QString &clientId)
{
    const QString id = clientId.trimmed().isEmpty() ? QStringLiteral("anonymous") : clientId.trimmed();
    const bool wasEmpty = m_clients.isEmpty();
    m_clients.insert(id);
    if (wasEmpty)
        startServer();
}

void NotificationCenterService::releaseClient(const QString &clientId)
{
    const QString id = clientId.trimmed().isEmpty() ? QStringLiteral("anonymous") : clientId.trimmed();
    m_clients.remove(id);
    if (m_clients.isEmpty())
        stopServer();
}

void NotificationCenterService::refresh()
{
    if (!m_clients.isEmpty())
        startServer();
}

void NotificationCenterService::setMode(const QString &mode)
{
    const QString normalized = normalizedMode(mode);
    if (normalized.isEmpty() || normalized == m_mode)
        return;
    m_mode = normalized;
    persistMode();
    emit modeChanged();
}

void NotificationCenterService::cycleMode()
{
    if (m_mode == QStringLiteral("normal")) {
        setMode(QStringLiteral("low"));
    } else if (m_mode == QStringLiteral("low")) {
        setMode(QStringLiteral("dnd"));
    } else {
        setMode(QStringLiteral("normal"));
    }
}

bool NotificationCenterService::invokeNotification(int id, const QString &actionKey)
{
    const int index = indexForId(id);
    if (index < 0)
        return false;

    const int previousUnread = calculateUnreadCount();
    NotificationRecord record = m_records.at(index);
    const QString requestedAction = actionKey.trimmed().isEmpty() ? QStringLiteral("default") : actionKey.trimmed();
    const bool validAction = hasAction(id, requestedAction);
    if (!validAction) {
        m_records[index].unread = false;
        emitCollectionChanged(previousUnread);
        return false;
    }

    emit actionInvoked(record.id, requestedAction);
    if (!record.resident)
        closeNotificationAt(indexForId(record.id), DismissedByUser);
    else {
        m_records[index].unread = false;
        emitCollectionChanged(previousUnread);
    }
    return true;
}

bool NotificationCenterService::markRead(int id)
{
    const int index = indexForId(id);
    if (index < 0)
        return false;
    if (!m_records.at(index).unread)
        return true;
    const int previousUnread = calculateUnreadCount();
    m_records[index].unread = false;
    emitCollectionChanged(previousUnread);
    return true;
}

bool NotificationCenterService::dismissNotification(int id)
{
    const int index = indexForId(id);
    if (index < 0)
        return false;
    return closeNotificationAt(index, DismissedByUser);
}

int NotificationCenterService::clearAll()
{
    int count = 0;
    while (!m_records.isEmpty()) {
        closeNotificationAt(0, DismissedByUser);
        ++count;
    }
    return count;
}

bool NotificationCenterService::hasAction(int id, const QString &actionKey) const
{
    const QVariantMap item = notification(id);
    const QVariantList actions = item.value(QStringLiteral("actions")).toList();
    for (const QVariant &value : actions) {
        const QVariantMap action = value.toMap();
        if (action.value(QStringLiteral("key")).toString() == actionKey)
            return true;
    }
    return false;
}

QVariantMap NotificationCenterService::notification(int id) const
{
    const int index = indexForId(id);
    return index < 0 ? QVariantMap() : toMap(m_records.at(index));
}

void NotificationCenterService::setMaxNotifications(int value)
{
    const int normalized = std::max(1, value);
    if (normalized == m_maxNotifications)
        return;
    m_maxNotifications = normalized;
    emit maxNotificationsChanged();
    enforceMaxNotifications();
    emitCollectionChanged(calculateUnreadCount());
}

void NotificationCenterService::startServer()
{
    if (m_serverStarted)
        return;
    if (envDisablesDbus()) {
        setServerAvailable(false);
        clearLastError();
        return;
    }

    if (!m_adaptor) {
        m_adaptor = new NotificationServerAdaptor(this);
        connect(this, &NotificationCenterService::actionInvoked,
                m_adaptor, &NotificationServerAdaptor::ActionInvoked);
        connect(this, &NotificationCenterService::notificationClosed,
                m_adaptor, &NotificationServerAdaptor::NotificationClosed);
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        setServerAvailable(false);
        setLastError(QStringLiteral("Session D-Bus is not available"));
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        setServerAvailable(false);
        setLastError(QStringLiteral("org.freedesktop.Notifications is already owned"));
        return;
    }

    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this, QDBusConnection::ExportAdaptors)) {
        bus.unregisterService(QString::fromLatin1(kServiceName));
        setServerAvailable(false);
        setLastError(QStringLiteral("Failed to register notification D-Bus object"));
        return;
    }

    m_objectRegistered = true;
    m_serverStarted = true;
    setServerAvailable(true);
    clearLastError();
}

void NotificationCenterService::stopServer()
{
    if (!m_serverStarted && !m_objectRegistered)
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_objectRegistered)
        bus.unregisterObject(QString::fromLatin1(kObjectPath));
    if (m_serverStarted)
        bus.unregisterService(QString::fromLatin1(kServiceName));

    m_objectRegistered = false;
    m_serverStarted = false;
    setServerAvailable(false);
}

void NotificationCenterService::setServerAvailable(bool available)
{
    if (available == m_serverAvailable)
        return;
    m_serverAvailable = available;
    emit serverAvailableChanged();
}

void NotificationCenterService::setLastError(const QString &message)
{
    if (message == m_lastError)
        return;
    m_lastError = message;
    if (!message.isEmpty())
        qCWarning(lcNotifications).noquote() << message;
    emit lastErrorChanged();
}

void NotificationCenterService::clearLastError()
{
    setLastError(QString());
}

QVariantMap NotificationCenterService::toMap(const NotificationRecord &record) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), record.id);
    map.insert(QStringLiteral("appName"), record.appName);
    map.insert(QStringLiteral("appIcon"), record.appIcon);
    map.insert(QStringLiteral("summary"), record.summary);
    map.insert(QStringLiteral("body"), record.body);
    map.insert(QStringLiteral("actions"), record.actions);
    map.insert(QStringLiteral("createdAt"), record.createdAt.toMSecsSinceEpoch());
    map.insert(QStringLiteral("createdIso"), record.createdAt.toString(Qt::ISODate));
    map.insert(QStringLiteral("expireTimeout"), record.expireTimeout);
    map.insert(QStringLiteral("unread"), record.unread);
    map.insert(QStringLiteral("resident"), record.resident);
    map.insert(QStringLiteral("transient"), record.transient);
    map.insert(QStringLiteral("urgency"), record.urgency);
    map.insert(QStringLiteral("defaultActionLabel"), actionLabel(record, QStringLiteral("default")));
    return map;
}

int NotificationCenterService::indexForId(uint id) const
{
    for (int index = 0; index < m_records.size(); ++index) {
        if (m_records.at(index).id == id)
            return index;
    }
    return -1;
}

int NotificationCenterService::indexForId(int id) const
{
    return id <= 0 ? -1 : indexForId(static_cast<uint>(id));
}

uint NotificationCenterService::nextNotificationId()
{
    if (m_nextId == 0)
        m_nextId = 1;
    return m_nextId++;
}

int NotificationCenterService::calculateUnreadCount() const
{
    int count = 0;
    for (const NotificationRecord &record : m_records) {
        if (record.unread)
            ++count;
    }
    return count;
}

void NotificationCenterService::emitCollectionChanged(int previousUnreadCount)
{
    emit notificationsChanged();
    if (previousUnreadCount != calculateUnreadCount())
        emit unreadCountChanged();
}

bool NotificationCenterService::closeNotificationAt(int index, uint reason)
{
    if (index < 0 || index >= m_records.size())
        return false;
    const int previousUnread = calculateUnreadCount();
    const uint id = m_records.at(index).id;
    m_records.removeAt(index);
    emit notificationClosed(id, reason);
    emitCollectionChanged(previousUnread);
    return true;
}

void NotificationCenterService::enforceMaxNotifications()
{
    while (m_records.size() > m_maxNotifications)
        closeNotificationAt(m_records.size() - 1, DismissedByUser);
}

QString NotificationCenterService::actionLabel(const NotificationRecord &record, const QString &actionKey) const
{
    for (const QVariant &value : record.actions) {
        const QVariantMap action = value.toMap();
        if (action.value(QStringLiteral("key")).toString() == actionKey)
            return action.value(QStringLiteral("label")).toString();
    }
    return QString();
}

bool NotificationCenterService::hintBool(const QVariantMap &hints, const QString &key, bool fallback) const
{
    if (!hints.contains(key))
        return fallback;
    return unwrapHint(hints.value(key)).toBool();
}

int NotificationCenterService::hintInt(const QVariantMap &hints, const QString &key, int fallback) const
{
    if (!hints.contains(key))
        return fallback;
    bool ok = false;
    const int value = unwrapHint(hints.value(key)).toInt(&ok);
    return ok ? value : fallback;
}

void NotificationCenterService::persistMode() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromLatin1(kSettingsOrg),
                       QString::fromLatin1(kSettingsApp));
    settings.setValue(QStringLiteral("mode"), m_mode);
}

void NotificationCenterService::loadMode()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromLatin1(kSettingsOrg),
                       QString::fromLatin1(kSettingsApp));
    const QString stored = normalizedMode(settings.value(QStringLiteral("mode")).toString());
    if (!stored.isEmpty())
        m_mode = stored;
}

#include "NotificationCenterService.moc"
