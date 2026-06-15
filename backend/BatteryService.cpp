#include "BatteryService.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QVariantMap>
#include <QtGlobal>

#include <libudev.h>

namespace {
Q_LOGGING_CATEGORY(lcBattery, "archipelago.battery")

QString readSysfsTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    const QString value = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    return value;
}
}  // namespace

BatteryService::BatteryService(QObject *parent)
    : QObject(parent)
{
    setupBattery();
    setupUpower();
}

BatteryService::~BatteryService()
{
    if (m_batteryMonitor)
        udev_monitor_unref(m_batteryMonitor);
    if (m_udev)
        udev_unref(m_udev);
}

int BatteryService::capacity() const { return m_capacity; }
QString BatteryService::status() const { return m_status; }

void BatteryService::refresh()
{
    updateSysfs();
    updateUpower();
}

void BatteryService::handleBatteryMonitorEvent()
{
    if (!m_batteryMonitor)
        return;
    bool shouldRefresh = false;
    udev_device *device = nullptr;
    while ((device = udev_monitor_receive_device(m_batteryMonitor)) != nullptr) {
        shouldRefresh = true;
        udev_device_unref(device);
    }
    if (shouldRefresh)
        updateSysfs();
}

void BatteryService::handleUpowerBatteryChanged()
{
    updateUpower();
}

void BatteryService::handleUpowerPropertiesChanged(const QString &interfaceName,
                                                   const QVariantMap &changedProperties,
                                                   const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties)
    if (interfaceName != QStringLiteral("org.freedesktop.UPower.Device"))
        return;
    if (!changedProperties.contains(QStringLiteral("Percentage"))
        && !changedProperties.contains(QStringLiteral("State")))
        return;
    updateUpower();
}

void BatteryService::setupBattery()
{
    detectPowerSupplyPaths();
    updateSysfs();

    if (!m_udev) {
        m_udev = udev_new();
        if (!m_udev) {
            qCWarning(lcBattery) << "Failed to create udev context";
            return;
        }
    }

    m_batteryMonitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_batteryMonitor) {
        qCWarning(lcBattery) << "Failed to create udev monitor";
        return;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(m_batteryMonitor, "power_supply", nullptr) < 0
        || udev_monitor_enable_receiving(m_batteryMonitor) < 0) {
        qCWarning(lcBattery) << "Failed to enable udev monitor";
        udev_monitor_unref(m_batteryMonitor);
        m_batteryMonitor = nullptr;
        return;
    }

    const int monitorFd = udev_monitor_get_fd(m_batteryMonitor);
    if (monitorFd < 0) {
        qCWarning(lcBattery) << "Failed to get udev monitor fd";
        udev_monitor_unref(m_batteryMonitor);
        m_batteryMonitor = nullptr;
        return;
    }

    m_batteryNotifier = new QSocketNotifier(monitorFd, QSocketNotifier::Read, this);
    connect(m_batteryNotifier, &QSocketNotifier::activated, this, &BatteryService::handleBatteryMonitorEvent);
}

void BatteryService::detectPowerSupplyPaths()
{
    m_batteryPath.clear();
    m_acPath.clear();

    const QDir dir(QStringLiteral("/sys/class/power_supply"));
    const QFileInfoList supplies = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &supplyInfo : supplies) {
        const QString name = supplyInfo.fileName();
        const QString path = supplyInfo.absoluteFilePath();
        const QString type = readSysfsTextFile(path + QStringLiteral("/type"));

        if (m_batteryPath.isEmpty() && (type == QStringLiteral("Battery") || name.startsWith(QStringLiteral("BAT")))) {
            m_batteryPath = path;
            continue;
        }

        if (m_acPath.isEmpty()
            && (type == QStringLiteral("Mains") || type == QStringLiteral("USB")
                || name.startsWith(QStringLiteral("AC")) || name.startsWith(QStringLiteral("ADP")))) {
            m_acPath = path;
        }
    }
}

void BatteryService::setupUpower()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qCWarning(lcBattery) << "System DBus is not available for UPower";
        return;
    }

    QDBusInterface upower(QStringLiteral("org.freedesktop.UPower"),
                          QStringLiteral("/org/freedesktop/UPower"),
                          QStringLiteral("org.freedesktop.UPower"),
                          bus, this);
    if (!upower.isValid()) {
        qCWarning(lcBattery) << "UPower service is not available";
        return;
    }

    QDBusReply<QDBusObjectPath> reply = upower.call(QStringLiteral("GetDisplayDevice"));
    if (!reply.isValid()) {
        qCWarning(lcBattery) << "Failed to get UPower display device:" << reply.error().message();
        return;
    }

    m_upowerBatteryPath = reply.value().path();
    if (m_upowerBatteryPath.isEmpty() || m_upowerBatteryPath == QStringLiteral("/")) {
        qCWarning(lcBattery) << "Invalid UPower display device path";
        return;
    }

    const bool changedConnected = bus.connect(
        QStringLiteral("org.freedesktop.UPower"),
        m_upowerBatteryPath,
        QStringLiteral("org.freedesktop.UPower.Device"),
        QStringLiteral("Changed"),
        this,
        SLOT(handleUpowerBatteryChanged()));

    const bool propertiesConnected = bus.connect(
        QStringLiteral("org.freedesktop.UPower"),
        m_upowerBatteryPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(handleUpowerPropertiesChanged(QString, QVariantMap, QStringList)));

    if (!changedConnected && !propertiesConnected) {
        qCWarning(lcBattery) << "Failed to connect to UPower battery change signals";
        m_upowerBatteryPath.clear();
        return;
    }

    updateUpower();
}

void BatteryService::updateState(int capacity, const QString &status)
{
    const bool capacityDidChange = !m_hasState || capacity != m_capacity;
    const bool statusDidChange = !m_hasState || status != m_status;

    if (!capacityDidChange && !statusDidChange)
        return;

    m_capacity = capacity;
    m_status = status;
    m_hasState = true;

    if (capacityDidChange)
        emit capacityChanged();
    if (statusDidChange)
        emit statusChanged();
    emit batteryChanged(m_capacity, m_status);
}

QString BatteryService::upowerStateToStatus(uint state)
{
    switch (state) {
    case 1:
    case 5:
        return QStringLiteral("Charging");
    case 2:
    case 6:
        return QStringLiteral("Discharging");
    case 3:
        return QStringLiteral("Empty");
    case 4:
        return QStringLiteral("Full");
    default:
        return QStringLiteral("Unknown");
    }
}

void BatteryService::updateSysfs()
{
    int currentCap = m_capacity;
    QString currentStatus = m_status;

    if (!m_batteryPath.isEmpty()) {
        const QString capText = readSysfsTextFile(m_batteryPath + QStringLiteral("/capacity"));
        if (!capText.isEmpty())
            currentCap = capText.toInt();

        const QString statusText = readSysfsTextFile(m_batteryPath + QStringLiteral("/status"));
        if (!statusText.isEmpty())
            currentStatus = statusText;
    }

    if ((currentStatus.isEmpty() || currentStatus == QStringLiteral("Unknown")) && !m_acPath.isEmpty()) {
        const QString onlineText = readSysfsTextFile(m_acPath + QStringLiteral("/online"));
        if (!onlineText.isEmpty()) {
            const int online = onlineText.toInt();
            currentStatus = online > 0 ? QStringLiteral("Charging") : QStringLiteral("Discharging");
        }
    }

    updateState(currentCap, currentStatus);
}

void BatteryService::updateUpower()
{
    if (m_upowerBatteryPath.isEmpty())
        return;

    QDBusInterface batteryProps(QStringLiteral("org.freedesktop.UPower"),
                                 m_upowerBatteryPath,
                                 QStringLiteral("org.freedesktop.DBus.Properties"),
                                 QDBusConnection::systemBus(), this);
    if (!batteryProps.isValid()) {
        qCWarning(lcBattery) << "Failed to create UPower properties interface";
        return;
    }

    QDBusReply<QVariant> percentageReply = batteryProps.call(
        QStringLiteral("Get"), QStringLiteral("org.freedesktop.UPower.Device"), QStringLiteral("Percentage"));
    QDBusReply<QVariant> stateReply = batteryProps.call(
        QStringLiteral("Get"), QStringLiteral("org.freedesktop.UPower.Device"), QStringLiteral("State"));

    if (!percentageReply.isValid() || !stateReply.isValid()) {
        qCWarning(lcBattery) << "Failed to read UPower battery properties";
        return;
    }

    const int currentCap = qRound(percentageReply.value().toDouble());
    const QString currentStatus = upowerStateToStatus(stateReply.value().toUInt());
    updateState(currentCap, currentStatus);
}
