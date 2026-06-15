#include "NotificationService.h"

#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringLiteral>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcNotifications, "archipelago.notifications")

constexpr int kRestartDelayMs = 1200;
}  // namespace

NotificationService::NotificationService(QObject *parent)
    : QObject(parent)
{
    m_restartTimer.setSingleShot(true);
    m_restartTimer.setInterval(kRestartDelayMs);
    connect(&m_restartTimer, &QTimer::timeout, this, &NotificationService::startMonitor);
    startMonitor();
}

NotificationService::~NotificationService()
{
    m_shuttingDown = true;
    m_restartTimer.stop();
    if (m_monitor) {
        m_monitor->disconnect(this);
        m_monitor->terminate();
        m_monitor->waitForFinished(500);
        m_monitor->deleteLater();
        m_monitor = nullptr;
    }
}

void NotificationService::startMonitor()
{
    if (m_shuttingDown || m_monitor)
        return;

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("dbus-monitor"));
    if (executable.isEmpty()) {
        qCWarning(lcNotifications) << "dbus-monitor is not available; notification mirroring is disabled";
        return;
    }

    m_monitor = new QProcess(this);
    m_monitor->setProgram(executable);
    m_monitor->setArguments({
        QStringLiteral("--system"),
        QStringLiteral("--monitor"),
        QStringLiteral("type='method_call',interface='org.freedesktop.Notifications',member='Notify'")
    });

    connect(m_monitor, &QProcess::readyReadStandardOutput,
            this, &NotificationService::handleOutput);
    connect(m_monitor, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                m_monitor->deleteLater();
                m_monitor = nullptr;
                m_buffer.clear();
                m_captureActive = false;
                if (!m_shuttingDown)
                    m_restartTimer.start();
            });

    m_monitor->start();
}

void NotificationService::handleOutput()
{
    if (!m_monitor)
        return;
    processLines(m_buffer, m_monitor->readAllStandardOutput(),
                 [this](const QString &line) { handleLine(line); });
}

void NotificationService::processLines(QByteArray &buffer, const QByteArray &chunk,
                                        const std::function<void(const QString &)> &handler)
{
    buffer.append(chunk);
    while (true) {
        const int newlineIndex = buffer.indexOf('\n');
        if (newlineIndex < 0)
            break;
        const QByteArray rawLine = buffer.left(newlineIndex);
        buffer.remove(0, newlineIndex + 1);
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (!line.isEmpty())
            handler(line);
    }
}

QString NotificationService::decodeMonitorString(const QString &line) const
{
    static const QRegularExpression stringPattern(QStringLiteral("^\\s*string \"(.*)\"\\s*$"));
    const QRegularExpressionMatch match = stringPattern.match(line);
    if (!match.hasMatch())
        return QString();

    const QString escaped = match.captured(1);
    QString decoded;
    decoded.reserve(escaped.size());

    for (int i = 0; i < escaped.size(); ++i) {
        const QChar ch = escaped.at(i);
        if (ch != QLatin1Char('\\') || i + 1 >= escaped.size()) {
            decoded.append(ch);
            continue;
        }
        const QChar next = escaped.at(++i);
        if (next == QLatin1Char('n')) decoded.append(QLatin1Char('\n'));
        else if (next == QLatin1Char('r')) decoded.append(QLatin1Char('\r'));
        else if (next == QLatin1Char('t')) decoded.append(QLatin1Char('\t'));
        else decoded.append(next);
    }
    return decoded;
}

void NotificationService::handleLine(const QString &line)
{
    if (line.isEmpty())
        return;

    if (line.contains(QStringLiteral("member=Notify"))) {
        m_captureActive = true;
        m_captureStage = 0;
        m_pendingAppName.clear();
        m_pendingSummary.clear();
        m_pendingBody.clear();
        return;
    }

    if (!m_captureActive)
        return;

    switch (m_captureStage) {
    case 0:
        if (!line.startsWith(QStringLiteral("string ")))
            return;
        m_pendingAppName = decodeMonitorString(line);
        m_captureStage = 1;
        return;
    case 1:
        if (!line.startsWith(QStringLiteral("uint32 ")))
            return;
        m_captureStage = 2;
        return;
    case 2:
        if (!line.startsWith(QStringLiteral("string ")))
            return;
        m_captureStage = 3;
        return;
    case 3:
        if (!line.startsWith(QStringLiteral("string ")))
            return;
        m_pendingSummary = decodeMonitorString(line);
        m_captureStage = 4;
        return;
    case 4:
        if (!line.startsWith(QStringLiteral("string ")))
            return;
        m_pendingBody = decodeMonitorString(line);
        emit notificationReceived(m_pendingAppName, m_pendingSummary, m_pendingBody);
        m_captureActive = false;
        m_captureStage = -1;
        return;
    default:
        m_captureActive = false;
        m_captureStage = -1;
        return;
    }
}
