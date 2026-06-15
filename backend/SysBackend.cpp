#include "SysBackend.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

namespace {
constexpr int kHyprlandReconnectMs = 2000;
constexpr int kLyricsRestartMs = 3000;
}  // namespace

SysBackend::SysBackend(QObject *parent)
    : QObject(parent)
    , m_lyricsProcess(new QProcess(this))
    , m_lyricsRestartTimer(new QTimer(this))
{
    m_lyricsRestartTimer->setSingleShot(true);
    m_lyricsRestartTimer->setInterval(kLyricsRestartMs);

    setupHyprland();
    setupLyrics();
}

SysBackend::~SysBackend() = default;

QString SysBackend::lyricsCurrentLyric() const { return m_lyricsCurrentLyric; }
bool SysBackend::lyricsIsSynced() const { return m_lyricsIsSynced; }
QString SysBackend::lyricsBackendStatus() const { return m_lyricsBackendStatus; }

void SysBackend::handleHyprlandData()
{
    m_hyprBuffer.append(m_hyprSocket->readAll());
    while (m_hyprBuffer.contains('\n')) {
        const int idx = m_hyprBuffer.indexOf('\n');
        const QString line = QString::fromUtf8(m_hyprBuffer.left(idx)).trimmed();
        m_hyprBuffer.remove(0, idx + 1);

        if (line.startsWith(QStringLiteral("workspace>>")) || line.startsWith(QStringLiteral("workspacev2>>"))) {
            const QString data = line.split(QStringLiteral(">>")).last();
            const int wsId = data.split(QLatin1Char(',')).first().toInt();
            if (wsId > 0)
                emit workspaceChanged(wsId);
        }
    }
}

void SysBackend::setupHyprland()
{
    const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (signature.isEmpty())
        return;

    const QString xdgRuntime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    const QString path1 = QStringLiteral("%1/hypr/%2/.socket2.sock").arg(xdgRuntime, signature);
    const QString path2 = QStringLiteral("/tmp/hypr/%1/.socket2.sock").arg(signature);

    QString targetPath;
    if (QFile::exists(path1))
        targetPath = path1;
    else if (QFile::exists(path2))
        targetPath = path2;
    else
        return;

    m_hyprSocket = new QLocalSocket(this);
    connect(m_hyprSocket, &QLocalSocket::readyRead, this, &SysBackend::handleHyprlandData);
    connect(m_hyprSocket, &QLocalSocket::disconnected, this, [this, targetPath]() {
        QTimer::singleShot(kHyprlandReconnectMs, m_hyprSocket, [this, targetPath]() {
            m_hyprSocket->connectToServer(targetPath);
        });
    });
    m_hyprSocket->connectToServer(targetPath);
}

void SysBackend::setupLyrics()
{
    connect(m_lyricsRestartTimer, &QTimer::timeout, this, &SysBackend::startLyricsBackend);
    connect(m_lyricsProcess, &QProcess::readyReadStandardOutput, this, &SysBackend::handleLyricsReadyRead);
    connect(m_lyricsProcess, &QProcess::readyReadStandardError, this, &SysBackend::handleLyricsStderr);
    connect(m_lyricsProcess, &QProcess::stateChanged, this, &SysBackend::handleLyricsProcessStateChanged);
    connect(m_lyricsProcess, &QProcess::errorOccurred, this, &SysBackend::handleLyricsProcessError);
    connect(m_lyricsProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &SysBackend::handleLyricsProcessFinished);

    startLyricsBackend();
}

QString SysBackend::findLyricsBackendExecutable() const
{
    const QString homeDir = QDir::homePath();
    const QString quickshellConfigDir = homeDir + QStringLiteral("/.config/quickshell");
    const QString envPath = qEnvironmentVariable("QUICKSHELL_LYRICS_BACKEND");
    const QString pathExecutable = QStandardPaths::findExecutable(QStringLiteral("lyricsmpris"));
    QStringList candidates = {
        envPath,
        QStringLiteral("/usr/share/archipelago/bin/lyricsmpris"),
        quickshellConfigDir + QStringLiteral("/bin/lyricsmpris"),
        homeDir + QStringLiteral("/.local/bin/lyricsmpris"),
        pathExecutable
    };

    QDirIterator configIterator(quickshellConfigDir,
                                 QDir::Dirs | QDir::NoDotAndDotDot,
                                 QDirIterator::NoIteratorFlags);
    while (configIterator.hasNext())
        candidates.insert(1, configIterator.next() + QStringLiteral("/bin/lyricsmpris"));

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty())
            continue;
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile() && info.isExecutable())
            return info.absoluteFilePath();
    }
    return QString();
}

void SysBackend::setLyricsCurrentLyric(const QString &lyric)
{
    if (m_lyricsCurrentLyric == lyric)
        return;
    m_lyricsCurrentLyric = lyric;
    emit lyricsCurrentLyricChanged();
}

void SysBackend::setLyricsIsSynced(bool synced)
{
    if (m_lyricsIsSynced == synced)
        return;
    m_lyricsIsSynced = synced;
    emit lyricsIsSyncedChanged();
}

void SysBackend::setLyricsBackendStatus(const QString &status)
{
    if (m_lyricsBackendStatus == status)
        return;
    m_lyricsBackendStatus = status;
    emit lyricsBackendStatusChanged();
}

void SysBackend::startLyricsBackend()
{
    if (!m_lyricsProcess)
        return;
    if (m_lyricsProcess->state() != QProcess::NotRunning)
        return;

    m_lyricsExecutablePath = findLyricsBackendExecutable();
    if (m_lyricsExecutablePath.isEmpty()) {
        setLyricsCurrentLyric(QString());
        setLyricsIsSynced(false);
        setLyricsBackendStatus(QStringLiteral("missing"));
        return;
    }

    m_lyricsStdoutBuffer.clear();
    setLyricsCurrentLyric(QString());
    setLyricsIsSynced(false);
    setLyricsBackendStatus(QStringLiteral("starting"));

    m_lyricsProcess->setProgram(m_lyricsExecutablePath);
    m_lyricsProcess->setArguments({ QStringLiteral("--pipe") });
    m_lyricsProcess->start();
}

void SysBackend::handleLyricsReadyRead()
{
    if (!m_lyricsProcess)
        return;
    m_lyricsStdoutBuffer.append(m_lyricsProcess->readAllStandardOutput());

    while (m_lyricsStdoutBuffer.contains('\n')) {
        const int newlineIndex = m_lyricsStdoutBuffer.indexOf('\n');
        const QByteArray rawLine = m_lyricsStdoutBuffer.left(newlineIndex);
        m_lyricsStdoutBuffer.remove(0, newlineIndex + 1);

        const QString lyricLine = QString::fromUtf8(rawLine).trimmed();
        if (lyricLine.isEmpty()) {
            setLyricsCurrentLyric(QString());
            setLyricsIsSynced(false);
            if (m_lyricsProcess->state() == QProcess::Running)
                setLyricsBackendStatus(QStringLiteral("running"));
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(rawLine, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();
            const QString type = object.value(QStringLiteral("type")).toString();

            if (type == QLatin1String("status")) {
                const QString status = object.value(QStringLiteral("status")).toString();
                if (!status.isEmpty())
                    setLyricsBackendStatus(status);
                continue;
            }

            if (type == QLatin1String("line")) {
                const QString text = object.value(QStringLiteral("text")).toString();
                const bool synced = object.value(QStringLiteral("synced")).toBool(false);
                setLyricsCurrentLyric(text);
                setLyricsIsSynced(synced && !text.isEmpty());
                if (!text.isEmpty())
                    setLyricsBackendStatus(synced ? QStringLiteral("synced") : QStringLiteral("plain"));
                else if (m_lyricsProcess->state() == QProcess::Running)
                    setLyricsBackendStatus(QStringLiteral("running"));
                continue;
            }
        }

        setLyricsCurrentLyric(lyricLine);
        setLyricsIsSynced(true);
        setLyricsBackendStatus(QStringLiteral("synced"));
    }
}

void SysBackend::handleLyricsProcessStateChanged(QProcess::ProcessState state)
{
    if (state == QProcess::Running && !m_lyricsIsSynced)
        setLyricsBackendStatus(QStringLiteral("running"));
}

void SysBackend::handleLyricsProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    setLyricsCurrentLyric(QString());
    setLyricsIsSynced(false);

    if (!m_lyricsExecutablePath.isEmpty()) {
        setLyricsBackendStatus(QStringLiteral("error"));
        m_lyricsRestartTimer->start();
    }
}

void SysBackend::handleLyricsProcessError(QProcess::ProcessError error)
{
    setLyricsCurrentLyric(QString());
    setLyricsIsSynced(false);

    if (error == QProcess::FailedToStart) {
        setLyricsBackendStatus(QStringLiteral("missing"));
        return;
    }

    setLyricsBackendStatus(QStringLiteral("error"));
    if (m_lyricsRestartTimer && !m_lyricsRestartTimer->isActive())
        m_lyricsRestartTimer->start();
}

void SysBackend::handleLyricsStderr()
{
    if (!m_lyricsProcess)
        return;
    const QString stderrText = QString::fromUtf8(m_lyricsProcess->readAllStandardError()).trimmed();
    if (stderrText.isEmpty())
        return;
    qWarning().noquote() << "[Lyrics]" << stderrText;
}
