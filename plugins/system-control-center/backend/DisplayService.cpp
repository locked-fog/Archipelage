#include "DisplayService.h"

#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <memory>

namespace {

constexpr int kRefreshIntervalMs = 10000;
constexpr int kCommandTimeoutMs = 3000;

}

DisplayService::DisplayService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &DisplayService::refresh);
}

bool DisplayService::available() const { return m_available; }
QString DisplayService::activeDeviceId() const { return m_activeDeviceId; }
double DisplayService::brightness() const { return m_brightness; }
QVariantList DisplayService::devices() const { return m_devices; }
QString DisplayService::lastError() const { return m_lastError; }

QStringList DisplayService::parseDeviceList(const QString &text)
{
    QStringList devices;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    static const QRegularExpression pattern(
        QStringLiteral("^Device '([^']+)' of class 'backlight':$"));

    for (const QString &line : lines) {
        const QRegularExpressionMatch match = pattern.match(line.trimmed());
        if (match.hasMatch())
            devices.append(match.captured(1));
    }
    return devices;
}

QVariantMap DisplayService::parseDeviceInfo(const QString &deviceId, const QString &text)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), deviceId);
    QString displayName = deviceId;
    displayName.replace(QLatin1Char('_'), QLatin1Char(' '));
    entry.insert(QStringLiteral("name"), displayName);
    entry.insert(QStringLiteral("brightness"), 0.0);
    entry.insert(QStringLiteral("percent"), 0);

    const QString firstLine = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts).value(0).trimmed();
    if (firstLine.isEmpty())
        return entry;

    const QStringList fields = firstLine.split(QLatin1Char(','));
    if (fields.size() < 5)
        return entry;

    bool currentOk = false;
    bool maxOk = false;
    const double current = fields.at(2).toDouble(&currentOk);
    const double maximum = fields.at(4).toDouble(&maxOk);
    if (!currentOk || !maxOk || maximum <= 0.0)
        return entry;

    const double normalized = std::clamp(current / maximum, 0.0, 1.0);
    entry.insert(QStringLiteral("brightness"), normalized);
    entry.insert(QStringLiteral("percent"), qRound(normalized * 100.0));
    return entry;
}

void DisplayService::registerClient(const QString &clientId)
{
    const QString normalized = clientId.trimmed();
    if (normalized.isEmpty() || m_clients.contains(normalized))
        return;
    m_clients.insert(normalized);
    start();
}

void DisplayService::releaseClient(const QString &clientId)
{
    m_clients.remove(clientId.trimmed());
    if (m_clients.isEmpty())
        stop();
}

void DisplayService::start()
{
    if (m_started)
        return;
    m_started = true;
    m_refreshTimer.start();
    refresh();
}

void DisplayService::stop()
{
    m_refreshTimer.stop();
    m_started = false;
}

void DisplayService::refresh()
{
    if (m_refreshInFlight) {
        m_refreshQueued = true;
        return;
    }

    m_refreshInFlight = true;
    runCommand(QStringLiteral("brightnessctl"),
               {QStringLiteral("--class=backlight"), QStringLiteral("--list")},
               [this](const CommandResult &listResult) {
        const QString listError = commandMessage(QStringLiteral("brightnessctl"), listResult);
        if (!listError.isEmpty()) {
            m_available = false;
            m_activeDeviceId.clear();
            m_brightness = 0.0;
            m_devices.clear();
            setLastError(listError);
            emit stateChanged();
            emit devicesChanged();
            finishRefresh();
            return;
        }

        const QStringList deviceIds = parseDeviceList(QString::fromUtf8(listResult.stdoutData));
        refreshDeviceDetails(deviceIds, 0, {});
    });
}

void DisplayService::setActiveDevice(const QString &deviceId)
{
    const QString normalized = deviceId.trimmed();
    if (normalized.isEmpty() || normalized == m_activeDeviceId)
        return;

    m_activeDeviceId = normalized;
    syncActiveDevice(m_devices);
    emit stateChanged();
    emit devicesChanged();
}

void DisplayService::setBrightness(double value)
{
    setDeviceBrightness(m_activeDeviceId, value);
}

void DisplayService::setDeviceBrightness(const QString &deviceId, double value)
{
    const QString normalizedId = deviceId.trimmed();
    if (normalizedId.isEmpty())
        return;

    const double clamped = std::clamp(value, 0.0, 1.0);
    runCommand(QStringLiteral("brightnessctl"),
               {QStringLiteral("--class=backlight"),
                QStringLiteral("--device=%1").arg(normalizedId),
                QStringLiteral("--quiet"),
                QStringLiteral("set"),
                QStringLiteral("%1%").arg(qRound(clamped * 100.0))},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("brightnessctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void DisplayService::runCommand(const QString &program,
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

void DisplayService::refreshDeviceDetails(const QStringList &deviceIds,
                                          int index,
                                          const QVariantList &currentDevices)
{
    if (index >= deviceIds.size()) {
        m_available = !currentDevices.isEmpty();
        m_devices = currentDevices;
        syncActiveDevice(m_devices);
        clearLastError();
        emit stateChanged();
        emit devicesChanged();
        finishRefresh();
        return;
    }

    const QString deviceId = deviceIds.at(index);
    runCommand(QStringLiteral("brightnessctl"),
               {QStringLiteral("--class=backlight"),
                QStringLiteral("--device=%1").arg(deviceId),
                QStringLiteral("--machine-readable"),
                QStringLiteral("info")},
               [this, deviceIds, index, currentDevices, deviceId](const CommandResult &result) {
        QVariantList nextDevices = currentDevices;
        const QString error = commandMessage(QStringLiteral("brightnessctl"), result);
        if (error.isEmpty())
            nextDevices.append(parseDeviceInfo(deviceId, QString::fromUtf8(result.stdoutData)));
        refreshDeviceDetails(deviceIds, index + 1, nextDevices);
    });
}

void DisplayService::syncActiveDevice(const QVariantList &devices)
{
    if (devices.isEmpty()) {
        m_activeDeviceId.clear();
        m_brightness = 0.0;
        return;
    }

    bool found = false;
    QVariantList updatedDevices;
    updatedDevices.reserve(devices.size());
    for (const QVariant &value : devices) {
        QVariantMap entry = value.toMap();
        const QString deviceId = entry.value(QStringLiteral("id")).toString();
        if (!found && (m_activeDeviceId.isEmpty() || deviceId == m_activeDeviceId)) {
            found = true;
            if (m_activeDeviceId.isEmpty())
                m_activeDeviceId = deviceId;
        }
    }

    if (!found)
        m_activeDeviceId = devices.constFirst().toMap().value(QStringLiteral("id")).toString();

    for (const QVariant &value : devices) {
        QVariantMap entry = value.toMap();
        const bool active = entry.value(QStringLiteral("id")).toString() == m_activeDeviceId;
        entry.insert(QStringLiteral("active"), active);
        if (active)
            m_brightness = entry.value(QStringLiteral("brightness")).toDouble();
        updatedDevices.append(entry);
    }

    m_devices = updatedDevices;
}

QString DisplayService::trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData)
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

QString DisplayService::commandMessage(const QString &program, const CommandResult &result) const
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

void DisplayService::finishRefresh()
{
    m_refreshInFlight = false;
    if (m_refreshQueued) {
        m_refreshQueued = false;
        refresh();
    }
}

void DisplayService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void DisplayService::clearLastError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}
