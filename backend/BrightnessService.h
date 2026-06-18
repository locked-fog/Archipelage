#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QtQml/qqml.h>

class QFileSystemWatcher;

// BrightnessService exposes screen backlight brightness in 0..1 to
// QML. It prefers querying and setting brightness through
// `brightnessctl --class=backlight` so device selection and write
// permissions follow the system's configured brightnessctl policy.
// If brightnessctl is unavailable or fails, it falls back to direct
// sysfs reads / writes under /sys/class/backlight.
class BrightnessService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(qreal brightness READ brightness NOTIFY brightnessChanged FINAL)

public:
    explicit BrightnessService(QObject *parent = nullptr);
    ~BrightnessService() override;

    qreal brightness() const;
    static qreal parseMachineReadableBrightness(const QString &text, bool *ok = nullptr);

    Q_INVOKABLE void requestBrightness();
    Q_INVOKABLE void setBrightness(qreal value);

signals:
    void brightnessChanged();

private:
    void setupBrightness();
    void detectBacklightPath();
    void updateBrightness();
    void requestBrightnessWithBrightnessctl();
    void setBrightnessWithBrightnessctl(qreal value);
    void startTimedProcess(QProcess *process,
                           QTimer *timeoutTimer,
                           const QString &program,
                           const QStringList &arguments);
    void handleBrightnessctlInfoFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleBrightnessctlSetFinished(int exitCode, QProcess::ExitStatus exitStatus);

    QFileSystemWatcher *m_watcher = nullptr;
    QProcess *m_brightnessInfoProcess = nullptr;
    QProcess *m_brightnessSetProcess = nullptr;
    QTimer *m_brightnessInfoTimeoutTimer = nullptr;
    QTimer *m_brightnessSetTimeoutTimer = nullptr;
    QString m_backlightPath;
    bool m_preferBrightnessctl = false;
    qreal m_brightness = 0.0;
    qreal m_maxBrightness = 1.0;
};
