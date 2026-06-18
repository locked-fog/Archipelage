#include "AudioRouteService.h"

#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <memory>

namespace {

constexpr int kRefreshIntervalMs = 3000;
constexpr int kCommandTimeoutMs = 3000;

}

AudioRouteService::AudioRouteService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AudioRouteService::refresh);
}

bool AudioRouteService::available() const { return m_available; }
double AudioRouteService::volume() const { return m_volume; }
bool AudioRouteService::muted() const { return m_muted; }
QString AudioRouteService::activeOutputName() const { return m_activeOutputName; }
bool AudioRouteService::bluetoothOutput() const { return m_bluetoothOutput; }
QVariantList AudioRouteService::outputs() const { return m_outputs; }
QVariantList AudioRouteService::streams() const { return m_streams; }
QString AudioRouteService::lastError() const { return m_lastError; }

void AudioRouteService::parseVolume(const QString &text, double *value, bool *muted, bool *ok)
{
    if (value)
        *value = -1.0;
    if (muted)
        *muted = false;
    if (ok)
        *ok = false;

    static const QRegularExpression pattern(QStringLiteral("([0-9]*\\.?[0-9]+)"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return;

    bool converted = false;
    const double parsedValue = match.captured(1).toDouble(&converted);
    if (!converted)
        return;

    if (value)
        *value = std::clamp(parsedValue, 0.0, 1.5);
    if (muted)
        *muted = text.contains(QStringLiteral("MUTED"), Qt::CaseInsensitive);
    if (ok)
        *ok = true;
}

AudioRouteService::StatusSnapshot AudioRouteService::parseStatus(const QString &text)
{
    StatusSnapshot snapshot;
    const QStringList lines = text.split(QLatin1Char('\n'));
    bool inAudio = false;
    bool inSinks = false;
    bool inStreams = false;
    static const QRegularExpression linePattern(
        QStringLiteral("^(\\*\\s*)?(\\d+)\\.\\s+(.+?)(?:\\s+\\[vol:\\s*([0-9]*\\.?[0-9]+)(?:\\s+MUTED)?\\])?$"));

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("Audio")) {
            inAudio = true;
            inSinks = false;
            inStreams = false;
            continue;
        }
        if (inAudio && trimmed == QStringLiteral("Video")) {
            inAudio = false;
            inSinks = false;
            inStreams = false;
            continue;
        }
        if (!inAudio) {
            continue;
        }
        if (trimmed.endsWith(QStringLiteral("Sinks:"))) {
            inSinks = true;
            inStreams = false;
            continue;
        }
        if (trimmed.endsWith(QStringLiteral("Streams:"))) {
            inSinks = false;
            inStreams = true;
            continue;
        }
        if (trimmed.endsWith(QStringLiteral("Sources:"))
            || trimmed.endsWith(QStringLiteral("Filters:"))
            || trimmed.endsWith(QStringLiteral("Devices:"))) {
            inSinks = false;
            inStreams = false;
            continue;
        }
        if ((!inSinks && !inStreams) || trimmed.isEmpty())
            continue;

        QString normalized = trimmed;
        while (!normalized.isEmpty()
               && !normalized.at(0).isDigit()
               && normalized.at(0) != QLatin1Char('*')) {
            normalized.remove(0, 1);
        }

        const bool active = normalized.startsWith(QLatin1Char('*'));
        if (active)
            normalized = normalized.mid(1).trimmed();
        const QRegularExpressionMatch match = linePattern.match(normalized);
        if (!match.hasMatch())
            continue;

        const QString nodeId = match.captured(2);
        const QString nodeName = match.captured(3);
        bool volumeOk = false;
        const double nodeVolume = match.captured(4).toDouble(&volumeOk);
        const bool nodeMuted = normalized.contains(QStringLiteral("MUTED"), Qt::CaseInsensitive);

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), nodeId);
        entry.insert(QStringLiteral("name"), nodeName);
        entry.insert(QStringLiteral("volume"), volumeOk ? nodeVolume : 0.0);
        entry.insert(QStringLiteral("muted"), nodeMuted);

        if (inSinks) {
            entry.insert(QStringLiteral("active"), active);
            entry.insert(QStringLiteral("bluetooth"), nodeName.contains(QStringLiteral("bluez")));
            snapshot.outputs.append(entry);

            if (active) {
                snapshot.activeOutputName = nodeName;
                snapshot.bluetoothOutput = nodeName.contains(QStringLiteral("bluez"));
            }
        } else if (inStreams) {
            snapshot.streams.append(entry);
        }
    }

    return snapshot;
}

void AudioRouteService::registerClient(const QString &clientId)
{
    const QString normalized = clientId.trimmed();
    if (normalized.isEmpty())
        return;
    if (m_clients.contains(normalized))
        return;
    m_clients.insert(normalized);
    start();
}

void AudioRouteService::releaseClient(const QString &clientId)
{
    m_clients.remove(clientId.trimmed());
    if (m_clients.isEmpty())
        stop();
}

void AudioRouteService::start()
{
    if (m_started)
        return;
    m_started = true;
    m_refreshTimer.start();
    refresh();
}

void AudioRouteService::stop()
{
    m_refreshTimer.stop();
    m_started = false;
}

void AudioRouteService::refresh()
{
    if (m_refreshInFlight) {
        m_refreshQueued = true;
        return;
    }

    m_refreshInFlight = true;
    runCommand(QStringLiteral("wpctl"),
               {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")},
               [this](const CommandResult &volumeResult) {
        const QString volumeError = commandMessage(QStringLiteral("wpctl"), volumeResult);
        if (!volumeError.isEmpty()) {
            m_available = false;
            setLastError(volumeError);
            finishRefresh();
            return;
        }

        double nextVolume = -1.0;
        bool nextMuted = false;
        bool ok = false;
        parseVolume(QString::fromUtf8(volumeResult.stdoutData), &nextVolume, &nextMuted, &ok);
        if (ok) {
            m_available = true;
            m_volume = nextVolume;
            m_muted = nextMuted;
        }

        runCommand(QStringLiteral("wpctl"),
                   {QStringLiteral("status"), QStringLiteral("-n")},
                   [this](const CommandResult &statusResult) {
            const QString statusError = commandMessage(QStringLiteral("wpctl"), statusResult);
            if (!statusError.isEmpty()) {
                setLastError(statusError);
                finishRefresh();
                return;
            }

            const StatusSnapshot snapshot = parseStatus(QString::fromUtf8(statusResult.stdoutData));
            m_outputs = snapshot.outputs;
            m_streams = snapshot.streams;
            m_activeOutputName = snapshot.activeOutputName;
            m_bluetoothOutput = snapshot.bluetoothOutput;
            clearLastError();
            emit stateChanged();
            emit outputsChanged();
            emit streamsChanged();
            finishRefresh();
        });
    });
}

void AudioRouteService::setVolume(double value)
{
    const double nextValue = std::clamp(value, 0.0, 1.5);
    runCommand(QStringLiteral("wpctl"),
               {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                QString::number(nextValue, 'f', 2)},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("wpctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void AudioRouteService::setMuted(bool muted)
{
    runCommand(QStringLiteral("wpctl"),
               {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                muted ? QStringLiteral("1") : QStringLiteral("0")},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("wpctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void AudioRouteService::setDefaultOutput(const QString &outputId)
{
    const QString normalized = outputId.trimmed();
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("wpctl"),
               {QStringLiteral("set-default"), normalized},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("wpctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void AudioRouteService::setStreamVolume(const QString &streamId, double value)
{
    const QString normalizedId = streamId.trimmed();
    if (normalizedId.isEmpty())
        return;

    const double nextValue = std::clamp(value, 0.0, 1.5);
    runCommand(QStringLiteral("wpctl"),
               {QStringLiteral("set-volume"), normalizedId, QString::number(nextValue, 'f', 2)},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("wpctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void AudioRouteService::runCommand(const QString &program,
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

QString AudioRouteService::trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData)
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

QString AudioRouteService::commandMessage(const QString &program, const CommandResult &result) const
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

void AudioRouteService::finishRefresh()
{
    m_refreshInFlight = false;
    if (m_refreshQueued) {
        m_refreshQueued = false;
        refresh();
    }
}

void AudioRouteService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void AudioRouteService::clearLastError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}
