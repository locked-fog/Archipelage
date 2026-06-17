#include "ConnectivityService.h"

#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <memory>

namespace {

constexpr int kRefreshIntervalMs = 8000;
constexpr int kCommandTimeoutMs = 4000;

}

ConnectivityService::ConnectivityService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ConnectivityService::refresh);
}

bool ConnectivityService::available() const { return m_available; }
bool ConnectivityService::wifiEnabled() const { return m_wifiEnabled; }
bool ConnectivityService::wifiConnected() const { return m_wifiConnected; }
bool ConnectivityService::wiredConnected() const { return m_wiredConnected; }
QString ConnectivityService::activeSsid() const { return m_activeSsid; }
QString ConnectivityService::wiredConnectionName() const { return m_wiredConnectionName; }
int ConnectivityService::activeSignal() const { return m_activeSignal; }
QString ConnectivityService::statusText() const { return m_statusText; }
QVariantList ConnectivityService::networks() const { return m_networks; }
QString ConnectivityService::lastError() const { return m_lastError; }

bool ConnectivityService::parseWifiEnabled(const QString &text)
{
    return text.trimmed().compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0;
}

QStringList ConnectivityService::splitEscapedFields(const QString &line)
{
    QStringList fields;
    QString current;
    bool escape = false;

    for (const QChar ch : line) {
        if (escape) {
            current.append(ch);
            escape = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escape = true;
            continue;
        }
        if (ch == QLatin1Char(':')) {
            fields.append(current);
            current.clear();
            continue;
        }
        current.append(ch);
    }

    fields.append(current);
    return fields;
}

QVariantList ConnectivityService::parseWifiList(const QString &text)
{
    QVariantList list;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = splitEscapedFields(line.trimmed());
        if (fields.size() < 4)
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("active"),
                     fields.at(0).compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
        entry.insert(QStringLiteral("ssid"), fields.at(1));
        entry.insert(QStringLiteral("signal"), fields.at(2).toInt());
        entry.insert(QStringLiteral("security"), fields.at(3));
        entry.insert(QStringLiteral("secure"), !fields.at(3).trimmed().isEmpty());
        list.append(entry);
    }
    return list;
}

ConnectivityService::WifiDeviceState ConnectivityService::parseWifiDeviceState(const QString &text)
{
    WifiDeviceState state;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = splitEscapedFields(line.trimmed());
        if (fields.size() < 4)
            continue;
        if (fields.at(1) == QStringLiteral("wifi")) {
            state.deviceName = fields.at(0);
            state.stateText = fields.at(2);
            state.connectionName = fields.at(3);
            state.connected = fields.at(2).startsWith(QStringLiteral("connected"));
        } else if (fields.at(1) == QStringLiteral("ethernet")
                   && fields.at(2).startsWith(QStringLiteral("connected"))) {
            state.wiredConnected = true;
            state.wiredConnectionName = fields.at(3);
        }
    }
    return state;
}

QVariantList ConnectivityService::parseSavedConnections(const QString &text)
{
    QVariantList list;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = splitEscapedFields(line.trimmed());
        if (fields.size() < 4)
            continue;
        if (fields.at(2) != QStringLiteral("802-11-wireless"))
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("ssid"), fields.at(0));
        entry.insert(QStringLiteral("uuid"), fields.at(1));
        entry.insert(QStringLiteral("autoconnect"),
                     fields.at(3).compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
        list.append(entry);
    }
    return list;
}

void ConnectivityService::registerClient(const QString &clientId)
{
    const QString normalized = clientId.trimmed();
    if (normalized.isEmpty())
        return;
    if (m_clients.contains(normalized))
        return;
    m_clients.insert(normalized);
    start();
}

void ConnectivityService::releaseClient(const QString &clientId)
{
    m_clients.remove(clientId.trimmed());
    if (m_clients.isEmpty())
        stop();
}

void ConnectivityService::start()
{
    if (m_started)
        return;
    m_started = true;
    m_refreshTimer.start();
    refresh();
}

void ConnectivityService::stop()
{
    m_refreshTimer.stop();
    m_started = false;
}

void ConnectivityService::refresh()
{
    if (m_refreshInFlight) {
        m_refreshQueued = true;
        return;
    }

    m_refreshInFlight = true;
    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("WIFI"),
                QStringLiteral("general"), QStringLiteral("status")},
               [this](const CommandResult &wifiEnabledResult) {
        const QString wifiEnabledError = commandMessage(QStringLiteral("nmcli"), wifiEnabledResult);
        if (!wifiEnabledError.isEmpty()) {
            m_available = false;
            m_wifiEnabled = false;
            m_wifiConnected = false;
            m_wiredConnected = false;
            m_activeSsid.clear();
            m_wiredConnectionName.clear();
            m_activeSignal = 0;
            m_statusText = QStringLiteral("Wi-Fi unavailable");
            m_networks.clear();
            setLastError(wifiEnabledError);
            emit stateChanged();
            emit networksChanged();
            finishRefresh();
            return;
        }

        m_available = true;
        m_wifiEnabled = parseWifiEnabled(QString::fromUtf8(wifiEnabledResult.stdoutData));

        runCommand(QStringLiteral("nmcli"),
                   {QStringLiteral("-t"), QStringLiteral("-e"), QStringLiteral("yes"),
                    QStringLiteral("-f"), QStringLiteral("ACTIVE,SSID,SIGNAL,SECURITY"),
                    QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list"),
                    QStringLiteral("--rescan"), QStringLiteral("no")},
                   [this](const CommandResult &wifiListResult) {
            const QString wifiListError = commandMessage(QStringLiteral("nmcli"), wifiListResult);
            if (!wifiListError.isEmpty()) {
                setLastError(wifiListError);
                finishRefresh();
                return;
            }

            const QVariantList nextNetworks = parseWifiList(QString::fromUtf8(wifiListResult.stdoutData));

            runCommand(QStringLiteral("nmcli"),
                       {QStringLiteral("-t"), QStringLiteral("-e"), QStringLiteral("yes"),
                        QStringLiteral("-f"), QStringLiteral("DEVICE,TYPE,STATE,CONNECTION"),
                        QStringLiteral("device"), QStringLiteral("status")},
                       [this, nextNetworks](const CommandResult &deviceResult) {
                const QString deviceError = commandMessage(QStringLiteral("nmcli"), deviceResult);
                if (!deviceError.isEmpty()) {
                    setLastError(deviceError);
                    finishRefresh();
                    return;
                }

                const WifiDeviceState nextDeviceState =
                    parseWifiDeviceState(QString::fromUtf8(deviceResult.stdoutData));
                const bool nextConnected = nextDeviceState.connected;
                QString nextSsid = nextDeviceState.connectionName;
                if (nextSsid.isEmpty()) {
                    for (const QVariant &value : nextNetworks) {
                        const QVariantMap entry = value.toMap();
                        if (entry.value(QStringLiteral("active")).toBool()) {
                            nextSsid = entry.value(QStringLiteral("ssid")).toString();
                            break;
                        }
                    }
                }
                runCommand(QStringLiteral("nmcli"),
                           {QStringLiteral("-t"), QStringLiteral("-e"), QStringLiteral("yes"),
                            QStringLiteral("-f"), QStringLiteral("NAME,UUID,TYPE,AUTOCONNECT"),
                            QStringLiteral("connection"), QStringLiteral("show")},
                           [this, nextNetworks, nextDeviceState, nextConnected, nextSsid](
                               const CommandResult &savedConnectionsResult) {
                    const QString connectionsError =
                        commandMessage(QStringLiteral("nmcli"), savedConnectionsResult);
                    if (!connectionsError.isEmpty()) {
                        setLastError(connectionsError);
                        finishRefresh();
                        return;
                    }

                    const QVariantList savedConnections =
                        parseSavedConnections(QString::fromUtf8(savedConnectionsResult.stdoutData));

                    QVariantMap savedBySsid;
                    for (const QVariant &value : savedConnections) {
                        const QVariantMap entry = value.toMap();
                        const QString ssid = entry.value(QStringLiteral("ssid")).toString();
                        if (!ssid.isEmpty())
                            savedBySsid.insert(ssid, entry);
                    }

                    QVariantList mergedNetworks;
                    int nextSignal = 0;
                    for (const QVariant &value : nextNetworks) {
                        QVariantMap entry = value.toMap();
                        const QString ssid = entry.value(QStringLiteral("ssid")).toString();
                        entry.insert(QStringLiteral("available"), true);

                        const QVariantMap savedEntry = savedBySsid.take(ssid).toMap();
                        const bool saved = !savedEntry.isEmpty();
                        entry.insert(QStringLiteral("saved"), saved);
                        entry.insert(QStringLiteral("autoconnect"),
                                     savedEntry.value(QStringLiteral("autoconnect")).toBool());
                        entry.insert(QStringLiteral("connectionUuid"),
                                     savedEntry.value(QStringLiteral("uuid")).toString());

                        if (entry.value(QStringLiteral("active")).toBool())
                            nextSignal = entry.value(QStringLiteral("signal")).toInt();
                        mergedNetworks.append(entry);
                    }

                    for (auto it = savedBySsid.cbegin(); it != savedBySsid.cend(); ++it) {
                        const QVariantMap savedEntry = it.value().toMap();
                        QVariantMap entry;
                        entry.insert(QStringLiteral("active"), false);
                        entry.insert(QStringLiteral("ssid"), savedEntry.value(QStringLiteral("ssid")).toString());
                        entry.insert(QStringLiteral("signal"), -1);
                        entry.insert(QStringLiteral("security"), QString());
                        entry.insert(QStringLiteral("secure"), false);
                        entry.insert(QStringLiteral("available"), false);
                        entry.insert(QStringLiteral("saved"), true);
                        entry.insert(QStringLiteral("autoconnect"),
                                     savedEntry.value(QStringLiteral("autoconnect")).toBool());
                        entry.insert(QStringLiteral("connectionUuid"),
                                     savedEntry.value(QStringLiteral("uuid")).toString());
                        mergedNetworks.append(entry);
                    }

                    m_wifiDeviceName = nextDeviceState.deviceName;
                    m_networks = mergedNetworks;
                    m_wifiConnected = nextConnected;
                    m_wiredConnected = nextDeviceState.wiredConnected;
                    m_activeSsid = nextSsid;
                    m_wiredConnectionName = nextDeviceState.wiredConnectionName;
                    m_activeSignal = nextSignal;

                    if (m_wiredConnected)
                        m_statusText = m_wiredConnectionName.isEmpty()
                            ? QStringLiteral("Ethernet")
                            : m_wiredConnectionName;
                    else if (!m_wifiEnabled)
                        m_statusText = QStringLiteral("Wi-Fi off");
                    else if (m_wifiConnected && !m_activeSsid.isEmpty())
                        m_statusText = m_activeSsid;
                    else if (!nextNetworks.isEmpty())
                        m_statusText = QStringLiteral("Ready");
                    else
                        m_statusText = QStringLiteral("No networks");

                    clearLastError();
                    emit stateChanged();
                    emit networksChanged();
                    finishRefresh();
                });
            });
        });
    });
}

void ConnectivityService::requestScan()
{
    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("rescan")},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("nmcli"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void ConnectivityService::setWifiEnabled(bool enabled)
{
    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("radio"), QStringLiteral("wifi"),
                enabled ? QStringLiteral("on") : QStringLiteral("off")},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("nmcli"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void ConnectivityService::connectToNetwork(const QString &ssid)
{
    const QString normalized = ssid.trimmed();
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("connect"), normalized},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("nmcli"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void ConnectivityService::disconnectActive()
{
    if (m_wifiDeviceName.isEmpty())
        return;

    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("device"), QStringLiteral("disconnect"), m_wifiDeviceName},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("nmcli"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void ConnectivityService::setAutoConnect(const QString &uuid, bool enabled)
{
    const QString normalized = uuid.trimmed();
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("connection"), QStringLiteral("modify"), normalized,
                QStringLiteral("connection.autoconnect"),
                enabled ? QStringLiteral("yes") : QStringLiteral("no")},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("nmcli"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void ConnectivityService::forgetNetwork(const QString &uuid)
{
    const QString normalized = uuid.trimmed();
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("nmcli"),
               {QStringLiteral("connection"), QStringLiteral("delete"), normalized},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("nmcli"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void ConnectivityService::runCommand(const QString &program,
                                     const QStringList &arguments,
                                     CommandCallback callback)
{
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        CommandResult result;
        result.errorString = QStringLiteral("%1 is not installed.").arg(program);
        QTimer::singleShot(0, this, [callback, result]() {
            callback(result);
        });
        return;
    }

    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    timeout->setInterval(kCommandTimeoutMs);

    process->setProgram(executable);
    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    auto completed = std::make_shared<bool>(false);
    auto finish = [process, timeout, callback, completed](const QString &errorString = QString()) {
        if (*completed)
            return;
        *completed = true;

        timeout->stop();
        CommandResult result;
        result.exitCode = process->exitCode();
        result.exitStatus = process->exitStatus();
        result.stdoutData = process->readAllStandardOutput();
        result.stderrData = process->readAllStandardError();
        result.errorString = errorString.isEmpty() ? process->errorString() : errorString;
        if (process->error() == QProcess::UnknownError)
            result.errorString.clear();
        callback(result);
        process->deleteLater();
    };

    connect(timeout, &QTimer::timeout, process, [process, finish]() {
        process->kill();
        finish(QStringLiteral("Command timed out."));
    });

    connect(process, &QProcess::errorOccurred, process, [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(QStringLiteral("Command failed to start."));
    });

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process,
            [finish](int, QProcess::ExitStatus) {
        finish();
    });

    process->start();
    timeout->start();
}

QString ConnectivityService::trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData)
{
    QString output = QString::fromUtf8(stdoutData).trimmed();
    const QString stderrOutput = QString::fromUtf8(stderrData).trimmed();
    if (!stderrOutput.isEmpty()) {
        if (!output.isEmpty())
            output += QLatin1Char('\n');
        output += stderrOutput;
    }
    return output;
}

QString ConnectivityService::commandMessage(const QString &program, const CommandResult &result) const
{
    if (!result.errorString.isEmpty())
        return result.errorString;
    if (result.exitStatus == QProcess::CrashExit)
        return QStringLiteral("%1 crashed.").arg(program);
    if (result.exitCode != 0) {
        const QString output = trimCommandOutput(result.stdoutData, result.stderrData);
        return output.isEmpty()
            ? QStringLiteral("%1 exited with code %2.").arg(program).arg(result.exitCode)
            : output;
    }
    return QString();
}

void ConnectivityService::finishRefresh()
{
    m_refreshInFlight = false;
    if (m_refreshQueued) {
        m_refreshQueued = false;
        refresh();
    }
}

void ConnectivityService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void ConnectivityService::clearLastError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}
