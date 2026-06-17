#include "CavaService.h"

#include "CavaSupport.h"

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cmath>

namespace {

constexpr int kBarCount = 8;
constexpr int kAsciiMaxRange = 1000;
constexpr int kRestartDelayMs = 1800;
constexpr qreal kLevelChangeThreshold = 0.03;

}  // namespace

CavaService::CavaService(QObject *parent)
    : QObject(parent)
{
    resetLevels();

    m_restartTimer.setSingleShot(true);
    m_restartTimer.setInterval(kRestartDelayMs);

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &CavaService::consumeOutput);
    connect(&m_process, &QProcess::stateChanged, this, &CavaService::handleStateChanged);
    connect(&m_process, &QProcess::errorOccurred, this, &CavaService::handleProcessError);
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &CavaService::handleProcessFinished);
    connect(&m_restartTimer, &QTimer::timeout, this, &CavaService::restartIfNeeded);
}

CavaService::~CavaService()
{
    stop();
}

bool CavaService::available() const
{
    if (disabledForTests())
        return false;
    return !m_executablePath.isEmpty() || !QStandardPaths::findExecutable(QStringLiteral("cava")).isEmpty();
}

bool CavaService::running() const
{
    return m_process.state() == QProcess::Running;
}

QVariantList CavaService::levels() const
{
    return m_levels;
}

QString CavaService::lastError() const
{
    return m_lastError;
}

void CavaService::registerClient(const QString &clientId)
{
    const bool wasEmpty = m_clients.isEmpty();
    m_clients.insert(normalizedClientId(clientId));
    if (wasEmpty)
        start();
}

void CavaService::releaseClient(const QString &clientId)
{
    m_clients.remove(normalizedClientId(clientId));
    if (m_clients.isEmpty())
        stop();
}

void CavaService::consumeOutput()
{
    m_stdoutBuffer.append(m_process.readAllStandardOutput());

    while (true) {
        const int newlineIndex = m_stdoutBuffer.indexOf('\n');
        if (newlineIndex < 0)
            break;

        const QByteArray frame = m_stdoutBuffer.left(newlineIndex);
        m_stdoutBuffer.remove(0, newlineIndex + 1);

        QVector<qreal> nextLevels;
        if (Cava::parseAsciiFrame(frame, kBarCount, kAsciiMaxRange, &nextLevels))
            setLevels(nextLevels);
    }
}

void CavaService::handleStateChanged(QProcess::ProcessState state)
{
    Q_UNUSED(state)
    emit runningChanged();
}

void CavaService::handleProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    setLastError(m_process.errorString());
    emit availabilityChanged();
    resetLevels();
    scheduleRestart();
}

void CavaService::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    if (m_process.state() == QProcess::NotRunning)
        emit runningChanged();

    resetLevels();
    const QByteArray stderrOutput = m_process.readAllStandardError().trimmed();
    if (m_process.exitStatus() == QProcess::CrashExit) {
        setLastError(QStringLiteral("cava crashed"));
    } else if (m_lastError.isEmpty() && !stderrOutput.isEmpty()) {
        setLastError(QString::fromUtf8(stderrOutput));
    }

    scheduleRestart();
}

void CavaService::restartIfNeeded()
{
    if (!m_clients.isEmpty() && !running())
        start();
}

bool CavaService::disabledForTests()
{
    return qEnvironmentVariableIsSet("ARCHIPELAGO_MEDIA_DISABLE_CAVA_FOR_TESTS");
}

QString CavaService::normalizedClientId(const QString &clientId)
{
    const QString trimmed = clientId.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("anonymous") : trimmed;
}

void CavaService::start()
{
    clearRestart();

    if (disabledForTests() || running()) {
        resetLevels();
        return;
    }

    if (executablePath().isEmpty()) {
        setLastError(QStringLiteral("cava executable not found"));
        emit availabilityChanged();
        resetLevels();
        return;
    }

    if (!ensureConfigReady()) {
        resetLevels();
        return;
    }

    m_stdoutBuffer.clear();
    clearLastError();
    m_process.setProgram(m_executablePath);
    m_process.setArguments({QStringLiteral("-p"), m_configDir->filePath(QStringLiteral("cava.conf"))});
    m_process.start();
}

void CavaService::stop()
{
    clearRestart();
    m_stdoutBuffer.clear();

    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(400))
            m_process.kill();
        m_process.waitForFinished(400);
    }

    resetLevels();
    clearLastError();
}

bool CavaService::ensureConfigReady()
{
    if (!m_configDir)
        m_configDir = std::make_unique<QTemporaryDir>();

    if (!m_configDir || !m_configDir->isValid()) {
        setLastError(QStringLiteral("failed to create a temporary cava config directory"));
        return false;
    }

    QFile configFile(m_configDir->filePath(QStringLiteral("cava.conf")));
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        setLastError(QStringLiteral("failed to write cava config"));
        return false;
    }

    Cava::Settings settings;
    settings.barCount = kBarCount;
    settings.asciiMaxRange = kAsciiMaxRange;
    settings.frameRate = 48;
    settings.sensitivity = 100;
    if (configFile.write(Cava::buildRawConfig(settings)) < 0) {
        setLastError(QStringLiteral("failed to persist cava config"));
        return false;
    }

    return true;
}

void CavaService::scheduleRestart()
{
    if (disabledForTests() || m_clients.isEmpty() || executablePath().isEmpty())
        return;
    if (!m_restartTimer.isActive())
        m_restartTimer.start();
}

void CavaService::clearRestart()
{
    if (m_restartTimer.isActive())
        m_restartTimer.stop();
}

void CavaService::resetLevels()
{
    setLevels(Cava::zeroLevels(kBarCount));
}

void CavaService::setLevels(const QVector<qreal> &levels)
{
    const QVariantList nextLevels = Cava::toVariantList(levels);
    if (m_levels.size() == nextLevels.size()) {
        bool changed = false;
        for (int index = 0; index < nextLevels.size(); ++index) {
            const qreal previous = m_levels.at(index).toReal();
            const qreal current = nextLevels.at(index).toReal();
            if (std::abs(previous - current) >= kLevelChangeThreshold) {
                changed = true;
                break;
            }
        }
        if (!changed)
            return;
    }
    m_levels = nextLevels;
    emit levelsChanged();
}

void CavaService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void CavaService::clearLastError()
{
    setLastError(QString());
}

QString CavaService::executablePath()
{
    if (!m_executablePath.isEmpty())
        return m_executablePath;
    m_executablePath = QStandardPaths::findExecutable(QStringLiteral("cava"));
    return m_executablePath;
}
