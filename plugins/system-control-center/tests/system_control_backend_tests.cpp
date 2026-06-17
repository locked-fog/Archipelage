#include <QtTest/QtTest>

#include "AudioRouteService.h"
#include "BluetoothService.h"
#include "ConnectivityService.h"
#include "DisplayService.h"
#include "PowerProfileService.h"

class SystemControlBackendTests : public QObject {
    Q_OBJECT

private slots:
    void connectivityParsesWifiList()
    {
        const auto list = ConnectivityService::parseWifiList(
            "yes:Office\\:5G:91:WPA2\n"
            "no:Cafe:44:\n");

        QCOMPARE(list.size(), 2);
        const QVariantMap first = list.at(0).toMap();
        const QVariantMap second = list.at(1).toMap();
        QCOMPARE(first.value(QStringLiteral("ssid")).toString(), QStringLiteral("Office:5G"));
        QCOMPARE(first.value(QStringLiteral("active")).toBool(), true);
        QCOMPARE(first.value(QStringLiteral("signal")).toInt(), 91);
        QCOMPARE(first.value(QStringLiteral("secure")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("secure")).toBool(), false);
    }

    void connectivityParsesWifiDeviceState()
    {
        const auto state = ConnectivityService::parseWifiDeviceState(
            "enp4s0:ethernet:connected:Wired connection 1\n"
            "wlan0:wifi:connected:Office\n");

        QCOMPARE(state.deviceName, QStringLiteral("wlan0"));
        QCOMPARE(state.stateText, QStringLiteral("connected"));
        QCOMPARE(state.connectionName, QStringLiteral("Office"));
        QCOMPARE(state.connected, true);
        QCOMPARE(state.wiredConnected, true);
        QCOMPARE(state.wiredConnectionName, QStringLiteral("Wired connection 1"));
    }

    void connectivityParsesSavedConnections()
    {
        const auto list = ConnectivityService::parseSavedConnections(
            "Office:uuid-office:802-11-wireless:yes\n"
            "Wired\\ connection\\ 1:uuid-wired:802-3-ethernet:yes\n");

        QCOMPARE(list.size(), 1);
        const QVariantMap first = list.constFirst().toMap();
        QCOMPARE(first.value(QStringLiteral("ssid")).toString(), QStringLiteral("Office"));
        QCOMPARE(first.value(QStringLiteral("uuid")).toString(), QStringLiteral("uuid-office"));
        QCOMPARE(first.value(QStringLiteral("autoconnect")).toBool(), true);
    }

    void bluetoothParsesShowInfo()
    {
        const auto state = BluetoothService::parseControllerState(
            "Controller 00:11:22:33:44:55 (public)\n"
            "\tPowered: yes\n"
            "\tDiscovering: no\n"
            "\tAlias: fogsarch\n");

        QCOMPARE(state.available, true);
        QCOMPARE(state.powered, true);
        QCOMPARE(state.discovering, false);
        QCOMPARE(state.alias, QStringLiteral("fogsarch"));
    }

    void bluetoothParsesDeviceInfo()
    {
        const auto device = BluetoothService::parseDeviceInfo(
            QStringLiteral("AA:BB:CC:DD:EE:FF"),
            "Device AA:BB:CC:DD:EE:FF Headphones\n"
            "\tAlias: Headphones\n"
            "\tPaired: yes\n"
            "\tTrusted: yes\n"
            "\tConnected: no\n"
            "\tIcon: audio-card\n"
            "\tRSSI: -54\n");

        QCOMPARE(device.value(QStringLiteral("address")).toString(), QStringLiteral("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(device.value(QStringLiteral("name")).toString(), QStringLiteral("Headphones"));
        QCOMPARE(device.value(QStringLiteral("paired")).toBool(), true);
        QCOMPARE(device.value(QStringLiteral("trusted")).toBool(), true);
        QCOMPARE(device.value(QStringLiteral("connected")).toBool(), false);
        QCOMPARE(device.value(QStringLiteral("icon")).toString(), QStringLiteral("audio-card"));
        QCOMPARE(device.value(QStringLiteral("rssi")).toInt(), -54);
    }

    void audioRouteParsesStatus()
    {
        const auto snapshot = AudioRouteService::parseStatus(
            "Audio\n"
            " ├─ Sinks:\n"
            " │  *   57. alsa_output.pci-0000_06_00.6.analog-stereo [vol: 0.55]\n"
            " │      91. bluez_output.11_22_33_44_55_66.1 [vol: 0.61]\n"
            " │\n"
            " ├─ Sources:\n");

        QCOMPARE(snapshot.outputs.size(), 2);
        const QVariantMap first = snapshot.outputs.at(0).toMap();
        const QVariantMap second = snapshot.outputs.at(1).toMap();
        QCOMPARE(first.value(QStringLiteral("id")).toString(), QStringLiteral("57"));
        QCOMPARE(first.value(QStringLiteral("active")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("bluetooth")).toBool(), true);
        QCOMPARE(snapshot.activeOutputName, QStringLiteral("alsa_output.pci-0000_06_00.6.analog-stereo"));
        QCOMPARE(snapshot.streams.size(), 0);
    }

    void audioRouteParsesStreams()
    {
        const auto snapshot = AudioRouteService::parseStatus(
            "Audio\n"
            " ├─ Sinks:\n"
            " │  *   57. alsa_output.pci-0000_06_00.6.analog-stereo [vol: 0.55]\n"
            " │\n"
            " └─ Streams:\n"
            "      91. Firefox [vol: 0.42]\n"
            "      92. mpv [vol: 0.75 MUTED]\n");

        QCOMPARE(snapshot.streams.size(), 2);
        const QVariantMap first = snapshot.streams.at(0).toMap();
        const QVariantMap second = snapshot.streams.at(1).toMap();
        QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("Firefox"));
        QCOMPARE(first.value(QStringLiteral("volume")).toDouble(), 0.42);
        QCOMPARE(second.value(QStringLiteral("muted")).toBool(), true);
    }

    void audioRouteParsesVolume()
    {
        double value = -1.0;
        bool muted = false;
        bool ok = false;

        AudioRouteService::parseVolume(QStringLiteral("Volume: 0.55 [MUTED]"), &value, &muted, &ok);

        QCOMPARE(ok, true);
        QCOMPARE(value, 0.55);
        QCOMPARE(muted, true);
    }

    void displayParsesDeviceListAndInfo()
    {
        const auto devices = DisplayService::parseDeviceList(
            "Available devices:\n"
            "Device 'intel_backlight' of class 'backlight':\n"
            "Device 'nvidia_wmi_ec_backlight' of class 'backlight':\n");

        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices.constFirst(), QStringLiteral("intel_backlight"));

        const QVariantMap device = DisplayService::parseDeviceInfo(
            QStringLiteral("intel_backlight"),
            "intel_backlight,backlight,448,56%,800\n");
        QCOMPARE(device.value(QStringLiteral("percent")).toInt(), 56);
        QCOMPARE(device.value(QStringLiteral("brightness")).toDouble(), 0.56);
    }

    void powerProfileParsesList()
    {
        const auto list = PowerProfileService::parseProfiles(
            "* performance:\n"
            "  balanced:\n"
            "  power-saver:\n");

        QCOMPARE(list.size(), 3);
        const QVariantMap first = list.constFirst().toMap();
        QCOMPARE(first.value(QStringLiteral("id")).toString(), QStringLiteral("performance"));
        QCOMPARE(first.value(QStringLiteral("active")).toBool(), true);
    }
};

QTEST_MAIN(SystemControlBackendTests)
#include "system_control_backend_tests.moc"
