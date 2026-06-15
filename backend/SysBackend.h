#pragma once

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QtQml/qqml.h>

// Legacy SysBackend, kept temporarily for the two subsystems that
// have not yet been migrated to their own QML singletons:
//   1. Hyprland IPC listener (kept as inert code until Hyprland
//      support is revived; the niri path lives in
//      NiriCompositorService, the only compositor in active use).
//   2. lyricsmpris subprocess that pipes synced / plain lyrics from
//      a separate binary into the media plugin's MediaState.
//      This will move to a dedicated MediaService in a follow-up
//      alongside the rest of the media-control rewrite.
//
// All other subsystems (battery, volume, brightness) have been moved
// to BatteryService, VolumeService, and BrightnessService.
class SysBackend : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString lyricsCurrentLyric READ lyricsCurrentLyric NOTIFY lyricsCurrentLyricChanged FINAL)
    Q_PROPERTY(bool lyricsIsSynced READ lyricsIsSynced NOTIFY lyricsIsSyncedChanged FINAL)
    Q_PROPERTY(QString lyricsBackendStatus READ lyricsBackendStatus NOTIFY lyricsBackendStatusChanged FINAL)

public:
    explicit SysBackend(QObject *parent = nullptr);
    ~SysBackend() override;

    QString lyricsCurrentLyric() const;
    bool lyricsIsSynced() const;
    QString lyricsBackendStatus() const;

signals:
    void workspaceChanged(int wsId);
    void lyricsCurrentLyricChanged();
    void lyricsIsSyncedChanged();
    void lyricsBackendStatusChanged();

private slots:
    void handleHyprlandData();
    void startLyricsBackend();
    void handleLyricsReadyRead();
    void handleLyricsProcessStateChanged(QProcess::ProcessState state);
    void handleLyricsProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleLyricsProcessError(QProcess::ProcessError error);
    void handleLyricsStderr();

private:
    void setupHyprland();
    void setupLyrics();
    QString findLyricsBackendExecutable() const;
    void setLyricsCurrentLyric(const QString &lyric);
    void setLyricsIsSynced(bool synced);
    void setLyricsBackendStatus(const QString &status);

    QLocalSocket *m_hyprSocket = nullptr;
    QByteArray m_hyprBuffer;
    QProcess *m_lyricsProcess = nullptr;
    QTimer *m_lyricsRestartTimer = nullptr;
    QByteArray m_lyricsStdoutBuffer;
    QString m_lyricsExecutablePath;
    QString m_lyricsCurrentLyric;
    QString m_lyricsBackendStatus = QStringLiteral("idle");
    bool m_lyricsIsSynced = false;
};
