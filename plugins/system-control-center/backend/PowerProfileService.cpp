#include "PowerProfileService.h"

#include <QRegularExpression>
#include <QStandardPaths>

#include <memory>

namespace {

constexpr int kRefreshIntervalMs = 12000;
constexpr int kCommandTimeoutMs = 3000;

}

PowerProfileService::PowerProfileService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &PowerProfileService::refresh);
}

bool PowerProfileService::available() const { return m_available; }
QString PowerProfileService::activeProfile() const { return m_activeProfile; }
QVariantList PowerProfileService::profiles() const { return m_profiles; }
QString PowerProfileService::lastError() const { return m_lastError; }

QString PowerProfileService::normalizeProfileName(const QString &text)
{
    return text.trimmed().toLower();
}

QVariantList PowerProfileService::parseProfiles(const QString &text)
{
    QVariantList list;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    static const QRegularExpression pattern(QStringLiteral("^([*\\s]*)\\s*([a-z-]+):\\s*$"));

    for (const QString &line : lines) {
        const QRegularExpressionMatch match = pattern.match(line.trimmed());
        if (!match.hasMatch())
            continue;

        const QString id = normalizeProfileName(match.captured(2));
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("label"),
                     id == QStringLiteral("power-saver")
                         ? QStringLiteral("Saver")
                         : (id == QStringLiteral("balanced")
                                ? QStringLiteral("Balanced")
                                : QStringLiteral("Performance")));
        entry.insert(QStringLiteral("active"), line.trimmed().startsWith(QLatin1Char('*')));
        list.append(entry);
    }

    return list;
}

void PowerProfileService::registerClient(const QString &clientId)
{
    const QString normalized = clientId.trimmed();
    if (normalized.isEmpty() || m_clients.contains(normalized))
        return;
    m_clients.insert(normalized);
    start();
}

void PowerProfileService::releaseClient(const QString &clientId)
{
    m_clients.remove(clientId.trimmed());
    if (m_clients.isEmpty())
        stop();
}

void PowerProfileService::start()
{
    if (m_started)
        return;
    m_started = true;
    m_refreshTimer.start();
    refresh();
}

void PowerProfileService::stop()
{
    m_refreshTimer.stop();
    m_started = false;
}

void PowerProfileService::refresh()
{
    if (m_refreshInFlight) {
        m_refreshQueued = true;
        return;
    }

    m_refreshInFlight = true;
    runCommand(QStringLiteral("powerprofilesctl"),
               {QStringLiteral("list")},
               [this](const CommandResult &listResult) {
        const QString listError = commandMessage(QStringLiteral("powerprofilesctl"), listResult);
        if (!listError.isEmpty()) {
            m_available = false;
            m_activeProfile.clear();
            m_profiles.clear();
            setLastError(listError);
            emit stateChanged();
            emit profilesChanged();
            finishRefresh();
            return;
        }

        const QVariantList nextProfiles = parseProfiles(QString::fromUtf8(listResult.stdoutData));
        runCommand(QStringLiteral("powerprofilesctl"),
                   {QStringLiteral("get")},
                   [this, nextProfiles](const CommandResult &getResult) {
            const QString getError = commandMessage(QStringLiteral("powerprofilesctl"), getResult);
            if (!getError.isEmpty()) {
                m_available = false;
                m_activeProfile.clear();
                m_profiles = nextProfiles;
                setLastError(getError);
                emit stateChanged();
                emit profilesChanged();
                finishRefresh();
                return;
            }

            const QString current = normalizeProfileName(QString::fromUtf8(getResult.stdoutData));
            QVariantList mergedProfiles;
            mergedProfiles.reserve(nextProfiles.size());
            for (const QVariant &value : nextProfiles) {
                QVariantMap entry = value.toMap();
                entry.insert(QStringLiteral("active"),
                             normalizeProfileName(entry.value(QStringLiteral("id")).toString()) == current);
                mergedProfiles.append(entry);
            }

            m_available = true;
            m_activeProfile = current;
            m_profiles = mergedProfiles;
            clearLastError();
            emit stateChanged();
            emit profilesChanged();
            finishRefresh();
        });
    });
}

void PowerProfileService::setProfile(const QString &profileId)
{
    const QString normalized = normalizeProfileName(profileId);
    if (normalized.isEmpty())
        return;

    runCommand(QStringLiteral("powerprofilesctl"),
               {QStringLiteral("set"), normalized},
               [this](const CommandResult &result) {
        const QString error = commandMessage(QStringLiteral("powerprofilesctl"), result);
        if (!error.isEmpty()) {
            setLastError(error);
            return;
        }
        clearLastError();
        refresh();
    });
}

void PowerProfileService::runCommand(const QString &program,
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

QString PowerProfileService::trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData)
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

QString PowerProfileService::commandMessage(const QString &program, const CommandResult &result) const
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

void PowerProfileService::finishRefresh()
{
    m_refreshInFlight = false;
    if (m_refreshQueued) {
        m_refreshQueued = false;
        refresh();
    }
}

void PowerProfileService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void PowerProfileService::clearLastError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}
