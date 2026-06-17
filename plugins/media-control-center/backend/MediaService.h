#pragma once

#include "MprisModel.h"

#include <QDBusContext>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>

class MediaService : public QObject, protected QDBusContext {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList players READ players NOTIFY playersChanged FINAL)
    Q_PROPERTY(QString activeService READ activeService NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString identity READ identity NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString artist READ artist NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString album READ album NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY stateChanged FINAL)
    Q_PROPERTY(qint64 duration READ duration NOTIFY stateChanged FINAL)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(double volume READ volume NOTIFY stateChanged FINAL)
    Q_PROPERTY(double rate READ rate NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString loopStatus READ loopStatus NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canPause READ canPause NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canControl READ canControl NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    explicit MediaService(QObject *parent = nullptr);

    bool available() const;
    QVariantList players() const;
    QString activeService() const;
    QString identity() const;
    QString playbackStatus() const;
    bool playing() const;
    QString title() const;
    QString artist() const;
    QString album() const;
    QString artUrl() const;
    qint64 duration() const;
    qint64 position() const;
    double volume() const;
    double rate() const;
    QString loopStatus() const;
    bool shuffle() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const;
    bool canPause() const;
    bool canSeek() const;
    bool canControl() const;
    QString lastError() const;

    Q_INVOKABLE void registerClient(const QString &clientId);
    Q_INVOKABLE void releaseClient(const QString &clientId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void choosePlayer(const QString &service);
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 offsetUs);
    Q_INVOKABLE void setPosition(qint64 positionUs);
    Q_INVOKABLE void setVolume(double value);
    Q_INVOKABLE void setLoopStatus(const QString &status);
    Q_INVOKABLE void toggleLoopStatus();
    Q_INVOKABLE void setShuffle(bool enabled);
    Q_INVOKABLE void toggleShuffle();

signals:
    void stateChanged();
    void playersChanged();
    void positionChanged();
    void lastErrorChanged();

private slots:
    void onPropertiesChanged(const QString &interfaceName,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void refreshPosition();

private:
    struct PlayerState {
        QString service;
        QString identity;
        QString playbackStatus;
        QVariantMap metadata;
        qint64 duration = 0;
        qint64 position = 0;
        double volume = 1.0;
        double rate = 1.0;
        QString loopStatus;
        bool shuffle = false;
        bool canGoNext = false;
        bool canGoPrevious = false;
        bool canPlay = false;
        bool canPause = false;
        bool canSeek = false;
        bool canControl = false;
    };

    void start();
    void stopService();
    void connectSignals();
    void disconnectSignals();
    QStringList discoverServices();
    PlayerState readPlayer(const QString &service);
    QVariant readProperty(const QString &service, const QString &interfaceName, const QString &propertyName);
    bool callPlayerMethod(const QString &method, const QVariantList &arguments = {});
    bool writePlayerProperty(const QString &propertyName, const QVariant &value);
    void setLastError(const QString &message);
    void clearLastError();
    void setActiveState(const PlayerState &state);
    QVariantList playerEntries(const QList<PlayerState> &states) const;
    QList<Mpris::PlayerInfo> playerInfoList(const QList<PlayerState> &states) const;

    QSet<QString> m_clients;
    QTimer m_positionTimer;
    QString m_requestedService;
    QVariantList m_players;
    PlayerState m_state;
    QString m_lastError;
    bool m_started = false;
    bool m_signalsConnected = false;
};
