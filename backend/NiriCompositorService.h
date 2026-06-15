#pragma once

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

class QJsonArray;
class QJsonObject;
class QJsonValue;

class NiriCompositorService : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(NiriCompositorService)
    QML_UNCREATABLE("NiriCompositorService is exposed by the Compositor singleton")

    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(QString socketPath READ socketPath NOTIFY socketPathChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY workspacesChanged FINAL)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY outputsChanged FINAL)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged FINAL)
    Q_PROPERTY(int focusedWorkspaceIdx READ focusedWorkspaceIdx NOTIFY focusedWorkspaceChanged FINAL)
    Q_PROPERTY(QVariantMap focusedWorkspace READ focusedWorkspace NOTIFY focusedWorkspaceChanged FINAL)
    Q_PROPERTY(qulonglong focusedWindowId READ focusedWindowId NOTIFY focusedWindowChanged FINAL)
    Q_PROPERTY(QVariantMap focusedWindow READ focusedWindow NOTIFY focusedWindowChanged FINAL)

public:
    explicit NiriCompositorService(QObject *parent = nullptr);
    ~NiriCompositorService() override;

    bool active() const;
    bool connected() const;
    QString socketPath() const;
    QString lastError() const;
    const QVariantList &workspaces() const;
    const QVariantList &outputs() const;
    const QVariantList &windows() const;
    int focusedWorkspaceIdx() const;
    QVariantMap focusedWorkspace() const;
    qulonglong focusedWindowId() const;
    QVariantMap focusedWindow() const;

    static QString detectSocketPath();
    static QVariantList parseWorkspaces(const QJsonArray &array);
    static QVariantList parseOutputs(const QJsonArray &array);
    static QVariantList parseWindows(const QJsonArray &array);
    static QString replySummary(const QJsonObject &root, const QString &context);

    bool applyEventForTest(const QJsonObject &event);

    Q_INVOKABLE void requestRefresh();
    Q_INVOKABLE QVariantList windowsForWorkspace(int workspaceIdx) const;
    Q_INVOKABLE void focusWorkspaceIdx(int idx);
    Q_INVOKABLE void focusWorkspaceName(const QString &name);
    Q_INVOKABLE void focusWorkspaceRelative(int delta);
    Q_INVOKABLE void focusWindow(qulonglong windowId);
    Q_INVOKABLE void closeWindow(qulonglong windowId);
    Q_INVOKABLE void closeFocusedWindow();
    Q_INVOKABLE void moveFocusedWindowToWorkspace(int workspaceIdx, bool focus);
    Q_INVOKABLE void moveWindowToWorkspace(qulonglong windowId, int workspaceIdx, bool focus);
    Q_INVOKABLE void moveWindowToWorkspaceByName(qulonglong windowId, const QString &workspaceName, bool focus);

signals:
    void activeChanged();
    void connectedChanged();
    void socketPathChanged();
    void lastErrorChanged();
    void workspacesChanged();
    void outputsChanged();
    void windowsChanged();
    void focusedWorkspaceChanged();
    void focusedWindowChanged();
    void workspaceActivated(int idx);
    void actionFailed(const QString &actionName, const QString &message);

private slots:
    void handleSocketReadyRead();
    void handleSocketDisconnected();
    void handleSocketError(QLocalSocket::LocalSocketError socketError);
    void handleReconnectTick();

private:
    struct PendingReply {
        QString kind;
        QString actionName;
    };

    void connectSocket();
    void scheduleReconnect();
    void startEventStream();
    void sendRequest(const QByteArray &payload, const PendingReply &pending);
    void sendAction(const QJsonObject &action, const QString &actionName);
    bool applyEvent(const QJsonObject &event);
    void processReply(const PendingReply &pending, const QJsonObject &root);
    void refreshSnapshot();
    void redetectSocketPath();
    void setConnected(bool connected);
    void setLastError(const QString &message);
    void setWorkspaces(const QVariantList &workspaces);
    void setOutputs(const QVariantList &outputs);
    void setWindows(const QVariantList &windows);
    void setFocusedWorkspaceIdx(int idx);
    void setFocusedWindowId(qulonglong windowId);
    QVariantMap resolveFocusedWorkspace() const;
    QVariantMap resolveFocusedWindow() const;
    QVariant workspaceIdForIdx(int workspaceIdx) const;

    QLocalSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QTimer m_reconnectTimer;
    QString m_socketPath;
    QString m_lastError;
    QVariantList m_workspaces;
    QVariantList m_outputs;
    QVariantList m_windows;
    QVariantMap m_focusedWorkspace;
    QVariantMap m_focusedWindow;
    int m_focusedWorkspaceIdx = 0;
    qulonglong m_focusedWindowId = 0;
    bool m_connected = false;
    bool m_eventStreamOpen = false;
    QQueue<PendingReply> m_pendingReplies;
};
