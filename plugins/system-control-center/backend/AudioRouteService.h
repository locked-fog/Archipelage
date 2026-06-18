#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <functional>

class AudioRouteService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(double volume READ volume NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool muted READ muted NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString activeOutputName READ activeOutputName NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool bluetoothOutput READ bluetoothOutput NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY outputsChanged FINAL)
    Q_PROPERTY(QVariantList streams READ streams NOTIFY streamsChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    struct StatusSnapshot {
        QVariantList outputs;
        QVariantList streams;
        QString activeOutputName;
        bool bluetoothOutput = false;
    };

    explicit AudioRouteService(QObject *parent = nullptr);

    bool available() const;
    double volume() const;
    bool muted() const;
    QString activeOutputName() const;
    bool bluetoothOutput() const;
    QVariantList outputs() const;
    QVariantList streams() const;
    QString lastError() const;

    static StatusSnapshot parseStatus(const QString &text);
    static void parseVolume(const QString &text, double *value, bool *muted, bool *ok);

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setVolume(double value);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void setDefaultOutput(const QString &outputId);
    Q_INVOKABLE void setStreamVolume(const QString &streamId, double value);

signals:
    void stateChanged();
    void outputsChanged();
    void streamsChanged();
    void lastErrorChanged();

private:
    struct CommandResult {
        int exitCode = -1;
        QProcess::ExitStatus exitStatus = QProcess::NormalExit;
        QByteArray stdoutData;
        QByteArray stderrData;
        QString errorString;
    };

    using CommandCallback = std::function<void(const CommandResult &)>;

    void start();
    void stop();
    void runCommand(const QString &program, const QStringList &arguments, CommandCallback callback);
    void finishRefresh();
    void setLastError(const QString &message);
    void clearLastError();
    QString commandMessage(const QString &program, const CommandResult &result) const;
    static QString trimCommandOutput(const QByteArray &stdoutData, const QByteArray &stderrData);

    QSet<QString> m_clients;
    QTimer m_refreshTimer;
    bool m_started = false;
    bool m_refreshInFlight = false;
    bool m_refreshQueued = false;
    bool m_available = false;
    double m_volume = 0.0;
    bool m_muted = false;
    QString m_activeOutputName;
    bool m_bluetoothOutput = false;
    QVariantList m_outputs;
    QVariantList m_streams;
    QString m_lastError;
};
