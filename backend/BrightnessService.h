#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

class QFileSystemWatcher;

// BrightnessService exposes screen backlight brightness in 0..1 to
// QML. It reads /sys/class/backlight/*/brightness and watches the
// file for changes. If no backlight device is available the
// brightness stays at 0 and setBrightness() is a no-op; future
// revisions may add a ddcutil or `light` fallback for laptops whose
// backlight is owned by the GPU.
class BrightnessService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(qreal brightness READ brightness NOTIFY brightnessChanged FINAL)

public:
    explicit BrightnessService(QObject *parent = nullptr);
    ~BrightnessService() override;

    qreal brightness() const;

    Q_INVOKABLE void requestBrightness();
    Q_INVOKABLE void setBrightness(qreal value);

signals:
    void brightnessChanged();

private:
    void setupBrightness();
    void detectBacklightPath();
    void updateBrightness();

    QFileSystemWatcher *m_watcher = nullptr;
    QString m_backlightPath;
    qreal m_brightness = 0.0;
    qreal m_maxBrightness = 1.0;
};
