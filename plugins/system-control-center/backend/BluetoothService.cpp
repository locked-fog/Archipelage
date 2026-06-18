#include "BluetoothService.h"

#include <QRegularExpression>
#include <QStandardPaths>

#include <memory>

namespace {

constexpr int kRefreshIntervalMs = 6000;
constexpr int kCommandTimeoutMs = 6000;
const QRegularExpression kDeviceLinePattern(
    QStringLiteral("^Device\\s+([0-9A-F:]+)\\s+(.+)$"),
    QRegularExpression::CaseInsensitiveOption);

}

BluetoothService::BluetoothService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &BluetoothService::refresh);
}

bool BluetoothService::available() const { return m_available; }
bool BluetoothService::powered() const { return m_powered; }
bool BluetoothService::discovering() const { return m_discovering; }
QString BluetoothService::connectedDeviceName() const { return m_connectedDeviceName; }
QString BluetoothService::statusText() const { return m_statusText; }
QVariantList BluetoothService::devices() const { return m_devices; }
QString BluetoothService::lastError() const { return m_lastError; }

BluetoothService::ControllerState BluetoothService::parseControllerState(const QString &text)
{
    ControllerState state;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Controller ")))
            state.available = true;
        else if (trimmed.startsWith(QStringLiteral("Powered:")))
            state.powered = trimmed.endsWith(QStringLiteral("yes"));
        else if (trimmed.startsWith(QStringLiteral("Discovering:")))
            state.discovering = trimmed.endsWith(QStringLiteral("yes"));
        else if (trimmed.startsWith(QStringLiteral("Alias:")))
            state.alias = trimmed.section(QStringLiteral(": "), 1);
    }
    return state;
}

QVariantMap BluetoothService::parseDeviceInfo(const QString &address, const QString &text)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("address"), address);
    entry.insert(QStringLiteral("name"), address);
    entry.insert(QStringLiteral("paired"), false);
    entry.insert(QStringLiteral("trusted"), false);
    entry.insert(QStringLiteral("connected"), false);
    entry.insert(QStringLiteral("icon"), QStringLiteral("bluetooth"));
    entry.insert(QStringLiteral("rssi"), -999);

    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Device "))) {
            const QRegularExpressionMatch match = kDeviceLinePattern.match(trimmed);
            if (match.hasMatch())
                entry.insert(QStringLiteral("name"), match.captured(2));
        } else if (trimmed.startsWith(QStringLiteral("Alias:"))) {
            entry.insert(QStringLiteral("name"), trimmed.section(QStringLiteral(": "), 1));
        } else if (trimmed.startsWith(QStringLiteral("Paired:"))) {
            entry.insert(QStringLiteral("paired"), trimmed.endsWith(QStringLiteral("yes")));
        } else if (trimmed.startsWith(QStringLiteral("Trusted:"))) {
            entry.insert(QStringLiteral("trusted"), trimmed.endsWith(QStringLiteral("yes")));
        } else if (trimmed.startsWith(QStringLiteral("Connected:"))) {
            entry.insert(QStringLiteral("connected"), trimmed.endsWith(QStringLiteral("yes")));
        } else if (trimmed.startsWith(QStringLiteral("Icon:"))) {
            entry.insert(QStringLiteral("icon"), trimmed.section(QStringLiteral(": "), 1));
        } else if (trimmed.startsWith(QStringLiteral("RSSI:"))) {
            entry.insert(QStringLiteral("rssi"), trimmed.section(QStringLiteral(": "), 1).toInt());
        }
    }

    return entry;
}

void BluetoothService::registerClient(const QString &clientId)
{
    const QString normalized = clientId.trimmed();
    if (normalized.isEmpty())
        return;
    if (m_clients.contains(normalized))
        return;
    m_clients.insert(normalized);
    start();
}

void BluetoothService::releaseClient(const QString &clientId)
{
    m_clients.remove(clientId.trimmed());
    if (m_clients.isEmpty())
        stop();
}

void BluetoothService::start()
{
    if (m_started)
        return;
    m_started = true;
    m_refreshTimer.start();
    refresh();
}

void BluetoothService::stop()
{
    m_refreshTimer.stop();
    m_started = false;
}

void BluetoothService::refresh()
{
    if (m_refreshInFlight) {
        m_refreshQueued = true;
        return;
    }

    m_refreshInFlight = true;
    runCommand(QStringLiteral("bluetoothctl"),
               {QStringLiteral("show")},
               [this](const CommandResult &showResult) {
        const QString showError = commandMessage(QStringLiteral("bluetoothctl"), showResult);
        if (!showError.isEmpty()) {
            m_available = false;
            m_powered = false;
            m_discovering = false;
            m_connectedDeviceName.clear();
            m_statusText = QStringLiteral("Bluetooth unavailable");
            m_devices.clear();
            setLastError(showError);
            emit stateChanged();
            emit devicesChanged();
            finishRefresh();
            return;
        }

        const ControllerState controllerState =
            parseControllerState(QString::fromUtf8(showResult.stdoutData));

        runCommand(QStringLiteral("bluetoothctl"),
                   {QStringLiteral("devices")},
                   [this, controllerState](const CommandResult &devicesResult) {
            const QString devicesError = commandMessage(QStringLiteral("bluetoothctl"), devicesResult);
            if (!devicesError.isEmpty()) {
                setLastError(devicesError);
                finishRefresh();
                return;
            }

            QStringList addresses;
            const QStringList lines =
                QString::fromUtf8(devicesResult.stdoutData).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QRegularExpressionMatch match = kDeviceLinePattern.match(line.trimmed());
                if (!match.hasMatch())
                    continue;
                addresses.append(match.captured(1));
            }

            refreshDeviceDetails(addresses, 0, {}, controllerState);
        });
    });
}

void BluetoothService::setPowered(bool enabled)
{
    runCommand(QStringLiteral("bluetoothctl"),
               {QStringLiteral("power"), enabled ? QStringLiteral("on") : QStringLiteral("off")},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("bluetoothctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void BluetoothService::requestScan()
{
    runCommand(QStringLiteral("bluetoothctl"),
               {QStringLiteral("--timeout"), QStringLiteral("6"),
                QStringLiteral("scan"), QStringLiteral("on")},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("bluetoothctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void BluetoothService::connectDevice(const QString &address)
{
    const QString normalized = address.trimmed().toUpper();
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("bluetoothctl"),
               {QStringLiteral("connect"), normalized},
               [this, normalized](const CommandResult &connectResult) {
        QString error = commandMessage(QStringLiteral("bluetoothctl"), connectResult);
        if (error.isEmpty()) {
            clearLastError();
            refresh();
            return;
        }

        runCommand(QStringLiteral("bluetoothctl"),
                   {QStringLiteral("pair"), normalized},
                   [this, normalized](const CommandResult &pairResult) {
            const QString pairError = commandMessage(QStringLiteral("bluetoothctl"), pairResult);
            if (!pairError.isEmpty()) {
                setLastError(pairError);
                return;
            }

            runCommand(QStringLiteral("bluetoothctl"),
                       {QStringLiteral("trust"), normalized},
                       [this, normalized](const CommandResult &) {
                runCommand(QStringLiteral("bluetoothctl"),
                           {QStringLiteral("connect"), normalized},
                           [this](const CommandResult &finalConnectResult) {
                    const QString finalError =
                        commandMessage(QStringLiteral("bluetoothctl"), finalConnectResult);
                    if (!finalError.isEmpty()) {
                        setLastError(finalError);
                        return;
                    }
                    clearLastError();
                    refresh();
                });
            });
        });
    });
}

void BluetoothService::disconnectDevice(const QString &address)
{
    const QString normalized = address.trimmed().toUpper();
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("bluetoothctl"),
               {QStringLiteral("disconnect"), normalized},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("bluetoothctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void BluetoothService::refreshDeviceDetails(const QStringList &addresses,
                                            int index,
                                            const QVariantList &currentDevices,
                                            const ControllerState &controllerState)
{
    if (index >= addresses.size()) {
        m_available = controllerState.available;
        m_powered = controllerState.powered;
        m_discovering = controllerState.discovering;
        m_devices = currentDevices;
        m_connectedDeviceName.clear();
        for (const QVariant &value : m_devices) {
            const QVariantMap entry = value.toMap();
            if (entry.value(QStringLiteral("connected")).toBool()) {
                m_connectedDeviceName = entry.value(QStringLiteral("name")).toString();
                break;
            }
        }

        if (!m_available)
            m_statusText = QStringLiteral("Bluetooth unavailable");
        else if (!m_powered)
            m_statusText = QStringLiteral("Bluetooth off");
        else if (!m_connectedDeviceName.isEmpty())
            m_statusText = m_connectedDeviceName;
        else if (m_discovering)
            m_statusText = QStringLiteral("Scanning");
        else
            m_statusText = QStringLiteral("Ready");

        clearLastError();
        emit stateChanged();
        emit devicesChanged();
        finishRefresh();
        return;
    }

    const QString address = addresses.at(index);
    runCommand(QStringLiteral("bluetoothctl"),
               {QStringLiteral("info"), address},
               [this, addresses, index, currentDevices, controllerState, address](const CommandResult &result) {
        QVariantList nextDevices = currentDevices;
        const QString error = commandMessage(QStringLiteral("bluetoothctl"), result);
        if (error.isEmpty())
            nextDevices.append(parseDeviceInfo(address, QString::fromUtf8(result.stdoutData)));
        refreshDeviceDetails(addresses, index + 1, nextDevices, controllerState);
    });
}

void BluetoothService::runCommand(const QString &program,
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

QString BluetoothService::trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData)
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

QString BluetoothService::commandMessage(const QString &program, const CommandResult &result) const
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

void BluetoothService::finishRefresh()
{
    m_refreshInFlight = false;
    if (m_refreshQueued) {
        m_refreshQueued = false;
        refresh();
    }
}

void BluetoothService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void BluetoothService::clearLastError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}
