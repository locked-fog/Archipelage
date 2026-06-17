#pragma once

#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <memory>

class QTemporaryDir;

class CavaService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged FINAL)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged FINAL)
    Q_PROPERTY(QVariantList levels READ levels NOTIFY levelsChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    explicit CavaService(QObject *parent = nullptr);
    ~CavaService() override;

    bool available() const;
    bool running() const;
    QVariantList levels() const;
    QString lastError() const;

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);

signals:
    void availabilityChanged();
    void runningChanged();
    void levelsChanged();
    void lastErrorChanged();

private slots:
    void consumeOutput();
    void handleStateChanged(QProcess::ProcessState state);
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void restartIfNeeded();

private:
    static bool disabledForTests();
    static QString normalizedClientId(const QString &clientId);

    void start();
    void stop();
    bool ensureConfigReady();
    void scheduleRestart();
    void clearRestart();
    void resetLevels();
    void setLevels(const QVector<qreal> &levels);
    void setLastError(const QString &message);
    void clearLastError();
    QString executablePath();

    QProcess m_process;
    QTimer m_restartTimer;
    QSet<QString> m_clients;
    QByteArray m_stdoutBuffer;
    QVariantList m_levels;
    QString m_lastError;
    QString m_executablePath;
    std::unique_ptr<QTemporaryDir> m_configDir;
};
