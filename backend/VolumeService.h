#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QtQml/qqml.h>

class QTimer;

// VolumeService exposes the default audio sink's volume and mute state
// to QML. It listens to pipewire / pulseaudio events (pactl subscribe)
// and re-queries on change, and detects whether the active sink is a
// bluetooth endpoint (sink name contains "bluez").
//
// This was extracted from the legacy SysBackend. The system plugin's
// compact view reads volume / muted through this singleton; future
// wheel handlers will route to its setters.
class VolumeService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(qreal volume READ volume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool bluetoothAudio READ bluetoothAudio NOTIFY bluetoothAudioChanged FINAL)

public:
    explicit VolumeService(QObject *parent = nullptr);
    ~VolumeService() override;

    qreal volume() const;
    bool muted() const;
    bool bluetoothAudio() const;

    Q_INVOKABLE void requestVolume();

signals:
    void volumeChanged();
    void bluetoothAudioChanged();

private slots:
    void handleVolumeEvent();
    void handleVolumeQueryFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleDefaultSinkQueryFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void startTimedProcess(QProcess *process, QTimer *timeoutTimer,
                           const QString &program, const QStringList &arguments);
    void fetchCurrentVolume();
    void checkDefaultAudioDevice();
    void handleAudioDebounce();

    QProcess *m_paSubscriber = nullptr;
    QProcess *m_volumeQuery = nullptr;
    QProcess *m_defaultSinkQuery = nullptr;
    QTimer *m_audioDebounceTimer = nullptr;
    QTimer *m_volumeQueryTimeoutTimer = nullptr;
    QTimer *m_defaultSinkQueryTimeoutTimer = nullptr;
    bool m_isBluetoothAudio = false;
    qreal m_volume = 0.0;
    bool m_muted = false;
};
