#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

class QSocketNotifier;

struct udev;
struct udev_monitor;

// BatteryService exposes the laptop battery state (capacity in
// percent, charging status string) to QML. It combines:
//   - sysfs /sys/class/power_supply polling
//   - UPower D-Bus properties (preferred when available)
//   - udev monitor on the power_supply subsystem for live updates
//
// This was extracted from the legacy SysBackend. External plugins can
// consume capacity / status through this singleton.
class BatteryService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int capacity READ capacity NOTIFY capacityChanged FINAL)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged FINAL)

public:
    explicit BatteryService(QObject *parent = nullptr);
    ~BatteryService() override;

    int capacity() const;
    QString status() const;

    // Maps a UPower State property value to the human-readable string
    // surfaced through Q_PROPERTY(status). Exposed public so unit tests
    // can verify the mapping without touching the live D-Bus service.
    static QString upowerStateToStatus(uint state);

    Q_INVOKABLE void refresh();

signals:
    void capacityChanged();
    void statusChanged();
    void batteryChanged(int capacity, const QString &status);

private slots:
    void handleBatteryMonitorEvent();
    void handleUpowerBatteryChanged();
    void handleUpowerPropertiesChanged(const QString &interfaceName,
                                       const QVariantMap &changedProperties,
                                       const QStringList &invalidatedProperties);

private:
    void setupBattery();
    void setupUpower();
    void detectPowerSupplyPaths();
    void updateSysfs();
    void updateUpower();
    void updateState(int capacity, const QString &status);

    QString m_batteryPath;
    QString m_acPath;
    int m_capacity = 0;
    QString m_status = QStringLiteral("Unknown");
    bool m_hasState = false;

    QString m_upowerBatteryPath;
    QSocketNotifier *m_batteryNotifier = nullptr;
    udev *m_udev = nullptr;
    udev_monitor *m_batteryMonitor = nullptr;
};
