#include "BrightnessService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcBrightness, "archipelago.brightness")

constexpr int kCommandTimeoutMs = 1200;

QString readSysfsTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    const QString value = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    return value;
}

bool writeSysfsTextFile(const QString &path, const QString &value)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream(&file) << value;
    file.close();
    return true;
}
}  // namespace

BrightnessService::BrightnessService(QObject *parent)
    : QObject(parent)
    , m_brightnessInfoProcess(new QProcess(this))
    , m_brightnessSetProcess(new QProcess(this))
    , m_brightnessInfoTimeoutTimer(new QTimer(this))
    , m_brightnessSetTimeoutTimer(new QTimer(this))
{
    m_brightnessInfoTimeoutTimer->setSingleShot(true);
    m_brightnessInfoTimeoutTimer->setInterval(kCommandTimeoutMs);
    m_brightnessSetTimeoutTimer->setSingleShot(true);
    m_brightnessSetTimeoutTimer->setInterval(kCommandTimeoutMs);

    connect(m_brightnessInfoTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_brightnessInfoProcess && m_brightnessInfoProcess->state() != QProcess::NotRunning)
            m_brightnessInfoProcess->kill();
    });
    connect(m_brightnessSetTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_brightnessSetProcess && m_brightnessSetProcess->state() != QProcess::NotRunning)
            m_brightnessSetProcess->kill();
    });

    connect(m_brightnessInfoProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &BrightnessService::handleBrightnessctlInfoFinished);
    connect(m_brightnessSetProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &BrightnessService::handleBrightnessctlSetFinished);

    connect(m_brightnessInfoProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_brightnessInfoTimeoutTimer)
            m_brightnessInfoTimeoutTimer->stop();
    });
    connect(m_brightnessSetProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_brightnessSetTimeoutTimer)
            m_brightnessSetTimeoutTimer->stop();
    });

    setupBrightness();
}

BrightnessService::~BrightnessService() = default;

qreal BrightnessService::brightness() const { return m_brightness; }

qreal BrightnessService::parseMachineReadableBrightness(const QString &text, bool *ok)
{
    if (ok)
        *ok = false;

    const QString firstLine = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts).value(0).trimmed();
    if (firstLine.isEmpty())
        return 0.0;

    const QStringList fields = firstLine.split(QLatin1Char(','));
    if (fields.size() < 5)
        return 0.0;

    bool currentOk = false;
    bool maxOk = false;
    const qreal current = fields.at(2).toDouble(&currentOk);
    const qreal maximum = fields.at(4).toDouble(&maxOk);
    if (!currentOk || !maxOk || maximum <= 0.0)
        return 0.0;

    if (ok)
        *ok = true;
    return qBound(0.0, current / maximum, 1.0);
}

void BrightnessService::requestBrightness()
{
    if (m_preferBrightnessctl) {
        requestBrightnessWithBrightnessctl();
        return;
    }
    updateBrightness();
}

void BrightnessService::setBrightness(qreal value)
{
    if (m_preferBrightnessctl) {
        setBrightnessWithBrightnessctl(value);
        return;
    }

    if (m_backlightPath.isEmpty() || m_maxBrightness <= 0.0)
        return;
    const qreal clamped = qBound(0.0, value, 1.0);
    const int target = qRound(clamped * m_maxBrightness);
    const QString brightnessFile = m_backlightPath + QStringLiteral("/brightness");
    writeSysfsTextFile(brightnessFile, QString::number(target));
    // updateBrightness() will be triggered by the file watcher's
    // fileChanged signal; no need to read back synchronously.
}

void BrightnessService::setupBrightness()
{
    m_preferBrightnessctl = !QStandardPaths::findExecutable(QStringLiteral("brightnessctl")).isEmpty();
    detectBacklightPath();
    if (!m_backlightPath.isEmpty()) {
        const QString maxBrightnessPath = m_backlightPath + QStringLiteral("/max_brightness");
        const QString brightnessPath = m_backlightPath + QStringLiteral("/brightness");

        const QString maxText = readSysfsTextFile(maxBrightnessPath);
        if (!maxText.isEmpty()) {
            bool ok = false;
            const qreal m = maxText.toDouble(&ok);
            if (ok && m > 0.0)
                m_maxBrightness = m;
        }

        if (QFileInfo::exists(brightnessPath)) {
            m_watcher = new QFileSystemWatcher(this);
            m_watcher->addPath(brightnessPath);
            connect(m_watcher, &QFileSystemWatcher::fileChanged,
                    this, &BrightnessService::updateBrightness);
        }
    }

    requestBrightness();
}

void BrightnessService::detectBacklightPath()
{
    m_backlightPath.clear();

    const QDir dir(QStringLiteral("/sys/class/backlight"));
    const QFileInfoList backlights = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    qreal bestMax = -1.0;
    for (const QFileInfo &info : backlights) {
        const QString candidatePath = info.absoluteFilePath();
        const QString maxText = readSysfsTextFile(candidatePath + QStringLiteral("/max_brightness"));
        const QString curText = readSysfsTextFile(candidatePath + QStringLiteral("/brightness"));
        if (maxText.isEmpty() || curText.isEmpty())
            continue;

        bool ok = false;
        const qreal max = maxText.toDouble(&ok);
        if (!ok || max <= 0.0)
            continue;

        if (max > bestMax) {
            bestMax = max;
            m_backlightPath = candidatePath;
        }
    }
}

void BrightnessService::updateBrightness()
{
    if (m_backlightPath.isEmpty())
        return;

    const QString brightnessFile = m_backlightPath + QStringLiteral("/brightness");
    const QString text = readSysfsTextFile(brightnessFile);
    if (text.isEmpty())
        return;

    bool ok = false;
    const qreal current = text.toDouble(&ok);
    if (!ok)
        return;

    if (m_maxBrightness <= 0.0)
        return;

    const qreal normalized = current / m_maxBrightness;
    if (qFuzzyCompare(normalized, m_brightness))
        return;
    m_brightness = normalized;
    emit brightnessChanged();
}

void BrightnessService::requestBrightnessWithBrightnessctl()
{
    startTimedProcess(m_brightnessInfoProcess,
                      m_brightnessInfoTimeoutTimer,
                      QStringLiteral("brightnessctl"),
                      {QStringLiteral("--class=backlight"),
                       QStringLiteral("--machine-readable"),
                       QStringLiteral("info")});
}

void BrightnessService::setBrightnessWithBrightnessctl(qreal value)
{
    const qreal clamped = qBound(0.0, value, 1.0);
    startTimedProcess(m_brightnessSetProcess,
                      m_brightnessSetTimeoutTimer,
                      QStringLiteral("brightnessctl"),
                      {QStringLiteral("--class=backlight"),
                       QStringLiteral("--quiet"),
                       QStringLiteral("set"),
                       QStringLiteral("%1%").arg(qRound(clamped * 100.0))});
}

void BrightnessService::startTimedProcess(QProcess *process,
                                          QTimer *timeoutTimer,
                                          const QString &program,
                                          const QStringList &arguments)
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

void BrightnessService::handleBrightnessctlInfoFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_brightnessInfoTimeoutTimer)
        m_brightnessInfoTimeoutTimer->stop();
    if (!m_brightnessInfoProcess || exitStatus != QProcess::NormalExit || exitCode != 0) {
        if (!m_backlightPath.isEmpty())
            updateBrightness();
        return;
    }

    bool ok = false;
    const qreal normalized =
        parseMachineReadableBrightness(QString::fromUtf8(m_brightnessInfoProcess->readAllStandardOutput()), &ok);
    if (!ok) {
        if (!m_backlightPath.isEmpty())
            updateBrightness();
        return;
    }

    if (qFuzzyCompare(normalized, m_brightness))
        return;
    m_brightness = normalized;
    emit brightnessChanged();
}

void BrightnessService::handleBrightnessctlSetFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_brightnessSetTimeoutTimer)
        m_brightnessSetTimeoutTimer->stop();
    if (!m_brightnessSetProcess || exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_preferBrightnessctl = false;
        requestBrightness();
        return;
    }

    requestBrightness();
}
