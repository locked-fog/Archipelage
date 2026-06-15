#include "VolumeService.h"

#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcVolume, "archipelago.volume")

constexpr int kCommandTimeoutMs = 500;
constexpr int kAudioEventDebounceMs = 80;
}  // namespace

VolumeService::VolumeService(QObject *parent)
    : QObject(parent)
    , m_paSubscriber(new QProcess(this))
    , m_volumeQuery(new QProcess(this))
    , m_defaultSinkQuery(new QProcess(this))
    , m_audioDebounceTimer(new QTimer(this))
    , m_volumeQueryTimeoutTimer(new QTimer(this))
    , m_defaultSinkQueryTimeoutTimer(new QTimer(this))
{
    m_volumeQueryTimeoutTimer->setSingleShot(true);
    m_volumeQueryTimeoutTimer->setInterval(kCommandTimeoutMs);
    m_defaultSinkQueryTimeoutTimer->setSingleShot(true);
    m_defaultSinkQueryTimeoutTimer->setInterval(kCommandTimeoutMs);
    m_audioDebounceTimer->setSingleShot(true);
    m_audioDebounceTimer->setInterval(kAudioEventDebounceMs);

    connect(m_paSubscriber, &QProcess::readyReadStandardOutput,
            this, &VolumeService::handleVolumeEvent);

    connect(m_volumeQueryTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_volumeQuery && m_volumeQuery->state() != QProcess::NotRunning)
            m_volumeQuery->kill();
    });
    connect(m_volumeQuery, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &VolumeService::handleVolumeQueryFinished);
    connect(m_volumeQuery, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_volumeQueryTimeoutTimer)
            m_volumeQueryTimeoutTimer->stop();
    });

    connect(m_defaultSinkQueryTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_defaultSinkQuery && m_defaultSinkQuery->state() != QProcess::NotRunning)
            m_defaultSinkQuery->kill();
    });
    connect(m_defaultSinkQuery, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &VolumeService::handleDefaultSinkQueryFinished);
    connect(m_defaultSinkQuery, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_defaultSinkQueryTimeoutTimer)
            m_defaultSinkQueryTimeoutTimer->stop();
    });

    connect(m_audioDebounceTimer, &QTimer::timeout, this, [this]() {
        fetchCurrentVolume();
        checkDefaultAudioDevice();
    });

    m_paSubscriber->start(QStringLiteral("pactl"), QStringList{ QStringLiteral("subscribe") });
    fetchCurrentVolume();
    checkDefaultAudioDevice();
}

VolumeService::~VolumeService() = default;

qreal VolumeService::volume() const { return m_volume; }
bool VolumeService::muted() const { return m_muted; }
bool VolumeService::bluetoothAudio() const { return m_isBluetoothAudio; }

void VolumeService::requestVolume()
{
    fetchCurrentVolume();
    checkDefaultAudioDevice();
}

void VolumeService::handleVolumeEvent()
{
    const QByteArray output = m_paSubscriber->readAllStandardOutput();
    if (output.contains("sink") || output.contains("card") || output.contains("server")) {
        if (m_audioDebounceTimer)
            m_audioDebounceTimer->start();
    }
}

void VolumeService::handleVolumeQueryFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_volumeQueryTimeoutTimer)
        m_volumeQueryTimeoutTimer->stop();
    if (!m_volumeQuery || exitStatus != QProcess::NormalExit || exitCode != 0)
        return;

    const QString output = QString::fromUtf8(m_volumeQuery->readAllStandardOutput()).trimmed();
    if (!output.startsWith(QStringLiteral("Volume:")))
        return;

    const bool muted = output.contains(QStringLiteral("[MUTED]"));
    const QString valStr = output.section(QLatin1Char(' '), 1, 1);
    bool ok = false;
    const qreal v = valStr.toDouble(&ok);
    if (!ok)
        return;

    if (qFuzzyCompare(v, m_volume) && muted == m_muted)
        return;
    m_volume = v;
    m_muted = muted;
    emit volumeChanged();
}

void VolumeService::handleDefaultSinkQueryFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_defaultSinkQueryTimeoutTimer)
        m_defaultSinkQueryTimeoutTimer->stop();
    if (!m_defaultSinkQuery || exitStatus != QProcess::NormalExit || exitCode != 0)
        return;

    const QString sinkName = QString::fromUtf8(m_defaultSinkQuery->readAllStandardOutput()).trimmed();
    const bool isBtNow = sinkName.contains(QStringLiteral("bluez"));

    if (isBtNow != m_isBluetoothAudio) {
        m_isBluetoothAudio = isBtNow;
        emit bluetoothAudioChanged();
    }
}

void VolumeService::fetchCurrentVolume()
{
    startTimedProcess(m_volumeQuery, m_volumeQueryTimeoutTimer,
                      QStringLiteral("wpctl"),
                      { QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@") });
}

void VolumeService::checkDefaultAudioDevice()
{
    startTimedProcess(m_defaultSinkQuery, m_defaultSinkQueryTimeoutTimer,
                      QStringLiteral("pactl"),
                      { QStringLiteral("get-default-sink") });
}

void VolumeService::startTimedProcess(QProcess *process, QTimer *timeoutTimer,
                                      const QString &program, const QStringList &arguments)
{
    if (!process || process->state() != QProcess::NotRunning)
        return;
    if (timeoutTimer)
        timeoutTimer->stop();
    process->setProgram(program);
    process->setArguments(arguments);
    process->start();
    if (timeoutTimer)
        timeoutTimer->start();
}

void VolumeService::handleAudioDebounce()
{
    fetchCurrentVolume();
    checkDefaultAudioDevice();
}
