#include "BrightnessService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QTextStream>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcBrightness, "archipelago.brightness")

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
{
    setupBrightness();
}

BrightnessService::~BrightnessService() = default;

qreal BrightnessService::brightness() const { return m_brightness; }

void BrightnessService::requestBrightness() { updateBrightness(); }

void BrightnessService::setBrightness(qreal value)
{
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
    detectBacklightPath();
    if (m_backlightPath.isEmpty())
        return;

    const QString maxBrightnessPath = m_backlightPath + QStringLiteral("/max_brightness");
    const QString brightnessPath = m_backlightPath + QStringLiteral("/brightness");

    const QString maxText = readSysfsTextFile(maxBrightnessPath);
    if (!maxText.isEmpty()) {
        bool ok = false;
        const qreal m = maxText.toDouble(&ok);
        if (ok && m > 0.0)
            m_maxBrightness = m;
    }

    if (!QFileInfo::exists(brightnessPath))
        return;

    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(brightnessPath);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &BrightnessService::updateBrightness);
    updateBrightness();
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
