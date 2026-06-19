#pragma once

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

class NotificationServerAdaptor;

class NotificationCenterService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool serverAvailable READ serverAvailable NOTIFY serverAvailableChanged FINAL)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged FINAL)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged FINAL)
    Q_PROPERTY(QVariantMap latestNotification READ latestNotification NOTIFY notificationsChanged FINAL)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged FINAL)
    Q_PROPERTY(int maxNotifications READ maxNotifications WRITE setMaxNotifications NOTIFY maxNotificationsChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    enum CloseReason : uint {
        Expired = 1,
        DismissedByUser = 2,
        ClosedByCall = 3,
        Undefined = 4,
    };
    Q_ENUM(CloseReason)

    explicit NotificationCenterService(QObject *parent = nullptr);
    ~NotificationCenterService() override;

    bool serverAvailable() const;
    QString mode() const;
    QVariantList notifications() const;
    QVariantMap latestNotification() const;
    int unreadCount() const;
    int maxNotifications() const;
    QString lastError() const;

    static QVariantList actionEntries(const QStringList &actions);
    static QString normalizedMode(const QString &mode);

    uint notify(const QString &appName,
                uint replacesId,
                const QString &appIcon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int expireTimeout);
    bool closeNotificationFromClient(uint id);

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void cycleMode();
    Q_INVOKABLE bool invokeNotification(int id, const QString &actionKey = QStringLiteral("default"));
    Q_INVOKABLE bool markRead(int id);
    Q_INVOKABLE bool dismissNotification(int id);
    Q_INVOKABLE int clearAll();
    Q_INVOKABLE bool hasAction(int id, const QString &actionKey) const;
    Q_INVOKABLE QVariantMap notification(int id) const;
    Q_INVOKABLE void setMaxNotifications(int value);

signals:
    void serverAvailableChanged();
    void modeChanged();
    void notificationsChanged();
    void unreadCountChanged();
    void maxNotificationsChanged();
    void lastErrorChanged();
    void notificationArrived(const QVariantMap &notification);
    void actionInvoked(uint id, const QString &actionKey);
    void notificationClosed(uint id, uint reason);

private:
    struct NotificationRecord {
        uint id = 0;
        QString appName;
        QString appIcon;
        QString summary;
        QString body;
        QVariantList actions;
        QVariantMap hints;
        QDateTime createdAt;
        int expireTimeout = -1;
        bool unread = true;
        bool resident = false;
        bool transient = false;
        int urgency = 1;
    };

    void startServer();
    void stopServer();
    void setServerAvailable(bool available);
    void setLastError(const QString &message);
    void clearLastError();
    QVariantMap toMap(const NotificationRecord &record) const;
    int indexForId(uint id) const;
    int indexForId(int id) const;
    uint nextNotificationId();
    int calculateUnreadCount() const;
    void emitCollectionChanged(int previousUnreadCount);
    bool closeNotificationAt(int index, uint reason);
    void enforceMaxNotifications();
    QString actionLabel(const NotificationRecord &record, const QString &actionKey) const;
    bool hintBool(const QVariantMap &hints, const QString &key, bool fallback = false) const;
    int hintInt(const QVariantMap &hints, const QString &key, int fallback = 0) const;
    void persistMode() const;
    void loadMode();

    QSet<QString> m_clients;
    QList<NotificationRecord> m_records;
    QString m_mode = QStringLiteral("normal");
    QString m_lastError;
    uint m_nextId = 1;
    int m_maxNotifications = 100;
    bool m_serverAvailable = false;
    bool m_serverStarted = false;
    bool m_objectRegistered = false;
    NotificationServerAdaptor *m_adaptor = nullptr;
};
