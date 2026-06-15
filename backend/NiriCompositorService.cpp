#include "NiriCompositorService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QLocalSocket>
#include <QSet>
#include <QVariant>
#include <algorithm>

namespace {
Q_LOGGING_CATEGORY(lcNiri, "archipelago.niri")

constexpr int kReconnectDelayMs = 2000;
constexpr int kRefreshIntervalMs = 4000;

QVariantMap windowLayoutMap(const QJsonObject &window)
{
    QVariantMap result;
    const QJsonValue layout = window.value(QStringLiteral("layout"));
    if (!layout.isObject())
        return result;

    const QJsonObject layoutObj = layout.toObject();
    const auto keys = layoutObj.keys();
    for (const QString &key : keys) {
        const QJsonValue variant = layoutObj.value(key);
        if (!variant.isObject())
            continue;
        if (key != QStringLiteral("Floating") && key != QStringLiteral("Tiled") && key != QStringLiteral("Pos"))
            continue;

        QVariantMap inner;
        const QJsonObject innerObj = variant.toObject();
        const auto innerKeys = innerObj.keys();
        for (const QString &innerKey : innerKeys)
            inner.insert(innerKey, innerObj.value(innerKey).toVariant());

        result.insert(key, inner);
    }
    return result;
}

QVariantList windowPoint(const QVariantMap &layout, const QStringList &keys)
{
    if (layout.isEmpty())
        return {0, 0};

    for (const QString &key : {QStringLiteral("Floating"), QStringLiteral("Tiled"), QStringLiteral("Pos")}) {
        const QVariant value = layout.value(key);
        if (!value.isValid() || !value.canConvert<QVariantMap>())
            continue;

        const QVariantMap inner = value.toMap();
        if (inner.contains(keys.at(0)) && inner.contains(keys.at(1)))
            return {inner.value(keys.at(0)).toInt(), inner.value(keys.at(1)).toInt()};
    }

    return {0, 0};
}

QVariantMap parseWorkspaceObject(const QJsonObject &object)
{
    QVariantMap map;
    const int idx = object.value(QStringLiteral("idx")).toInt();
    const QString name = object.value(QStringLiteral("name")).isString()
        ? object.value(QStringLiteral("name")).toString()
        : QString();

    map.insert(QStringLiteral("id"), object.value(QStringLiteral("id")).toVariant());
    map.insert(QStringLiteral("idx"), idx);
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("displayName"), name.isEmpty() ? QString::number(idx) : name);
    map.insert(QStringLiteral("output"),
               object.value(QStringLiteral("output")).isString()
                   ? object.value(QStringLiteral("output")).toString()
                   : QString());
    map.insert(QStringLiteral("isActive"), object.value(QStringLiteral("is_active")).toBool(false));
    map.insert(QStringLiteral("isFocused"), object.value(QStringLiteral("is_focused")).toBool(false));
    map.insert(QStringLiteral("activeWindowId"), object.value(QStringLiteral("active_window_id")).toVariant());
    return map;
}

QVariantMap parseOutputObject(const QJsonObject &object)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), object.value(QStringLiteral("name")).toString());
    if (object.value(QStringLiteral("logical")).isObject()) {
        const QJsonObject logical = object.value(QStringLiteral("logical")).toObject();
        map.insert(QStringLiteral("x"), logical.value(QStringLiteral("x")).toInt());
        map.insert(QStringLiteral("y"), logical.value(QStringLiteral("y")).toInt());
        map.insert(QStringLiteral("width"), logical.value(QStringLiteral("width")).toInt());
        map.insert(QStringLiteral("height"), logical.value(QStringLiteral("height")).toInt());
    } else {
        map.insert(QStringLiteral("x"), 0);
        map.insert(QStringLiteral("y"), 0);
        map.insert(QStringLiteral("width"), 0);
        map.insert(QStringLiteral("height"), 0);
    }
    map.insert(QStringLiteral("scale"), object.value(QStringLiteral("scale")).toDouble(1.0));
    map.insert(QStringLiteral("transform"), object.value(QStringLiteral("transform")).toString(QStringLiteral("Normal")));
    return map;
}

QVariantMap parseWindowObject(const QJsonObject &object)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), object.value(QStringLiteral("id")).toVariant());
    map.insert(QStringLiteral("title"), object.value(QStringLiteral("title")).toString());
    map.insert(QStringLiteral("appId"), object.value(QStringLiteral("app_id")).toString());
    map.insert(QStringLiteral("workspaceId"), object.value(QStringLiteral("workspace_id")).toVariant());
    map.insert(QStringLiteral("isFocused"), object.value(QStringLiteral("is_focused")).toBool(false));
    map.insert(QStringLiteral("isFloating"), object.value(QStringLiteral("is_floating")).toBool(false));

    const QVariantMap layout = windowLayoutMap(object);
    map.insert(QStringLiteral("at"), windowPoint(layout, {QStringLiteral("x"), QStringLiteral("y")}));
    map.insert(QStringLiteral("size"), windowPoint(layout, {QStringLiteral("width"), QStringLiteral("height")}));
    return map;
}

QVariantList sortedWorkspaceIndexes(const QVariantList &workspaces)
{
    QVariantList indexes;
    indexes.reserve(workspaces.size());
    for (const QVariant &workspace : workspaces) {
        const int idx = workspace.toMap().value(QStringLiteral("idx")).toInt();
        if (idx > 0)
            indexes.append(idx);
    }
    std::sort(indexes.begin(), indexes.end(), [](const QVariant &a, const QVariant &b) {
        return a.toInt() < b.toInt();
    });
    return indexes;
}
}  // namespace

NiriCompositorService::NiriCompositorService(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_socketPath(detectSocketPath())
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(kReconnectDelayMs);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &NiriCompositorService::handleReconnectTick);

    connect(m_socket, &QLocalSocket::readyRead, this, &NiriCompositorService::handleSocketReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, this, &NiriCompositorService::handleSocketDisconnected);
    connect(m_socket,
            qOverload<QLocalSocket::LocalSocketError>(&QLocalSocket::errorOccurred),
            this,
            &NiriCompositorService::handleSocketError);

    if (!m_socketPath.isEmpty())
        connectSocket();
    else
        setLastError(QStringLiteral("niri IPC socket was not found"));
}

NiriCompositorService::~NiriCompositorService()
{
    if (!m_socket)
        return;
    m_socket->disconnect(this);
    m_socket->abort();
}

bool NiriCompositorService::active() const { return m_connected && m_eventStreamOpen; }
bool NiriCompositorService::connected() const { return m_connected; }
QString NiriCompositorService::socketPath() const { return m_socketPath; }
QString NiriCompositorService::lastError() const { return m_lastError; }
const QVariantList &NiriCompositorService::workspaces() const { return m_workspaces; }
const QVariantList &NiriCompositorService::outputs() const { return m_outputs; }
const QVariantList &NiriCompositorService::windows() const { return m_windows; }
int NiriCompositorService::focusedWorkspaceIdx() const { return m_focusedWorkspaceIdx; }
QVariantMap NiriCompositorService::focusedWorkspace() const { return m_focusedWorkspace; }
qulonglong NiriCompositorService::focusedWindowId() const { return m_focusedWindowId; }
QVariantMap NiriCompositorService::focusedWindow() const { return m_focusedWindow; }

QString NiriCompositorService::detectSocketPath()
{
    const QByteArray env = qgetenv("NIRI_SOCKET");
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env);

    const QByteArray xdg = qgetenv("XDG_RUNTIME_DIR");
    if (!xdg.isEmpty()) {
        const QString candidate = QString::fromLocal8Bit(xdg) + QStringLiteral("/niri-ipc.sock");
        if (QFile::exists(candidate))
            return candidate;
    }

    return QString();
}

QVariantList NiriCompositorService::parseWorkspaces(const QJsonArray &array)
{
    QVariantList result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isObject())
            result.append(parseWorkspaceObject(value.toObject()));
    }
    return result;
}

QVariantList NiriCompositorService::parseOutputs(const QJsonArray &array)
{
    QVariantList result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isObject())
            result.append(parseOutputObject(value.toObject()));
    }
    return result;
}

QVariantList NiriCompositorService::parseWindows(const QJsonArray &array)
{
    QVariantList result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isObject())
            result.append(parseWindowObject(value.toObject()));
    }
    return result;
}

QString NiriCompositorService::replySummary(const QJsonObject &root, const QString &context)
{
    Q_UNUSED(context)
    if (root.contains(QStringLiteral("Err"))) {
        const QJsonValue err = root.value(QStringLiteral("Err"));
        if (err.isString())
            return QStringLiteral("error:") + err.toString();
        if (err.isObject())
            return QStringLiteral("error:") + QString::fromUtf8(QJsonDocument(err.toObject()).toJson(QJsonDocument::Compact));
        if (err.isArray())
            return QStringLiteral("error:") + QString::fromUtf8(QJsonDocument(err.toArray()).toJson(QJsonDocument::Compact));
        return QStringLiteral("error");
    }

    if (!root.contains(QStringLiteral("Ok")))
        return QString();

    const QJsonValue ok = root.value(QStringLiteral("Ok"));
    if (ok.isNull())
        return QStringLiteral("ok");
    if (ok.isArray())
        return QStringLiteral("array[") + QString::number(ok.toArray().size()) + QStringLiteral("]");
    if (ok.isObject())
        return QString::fromUtf8(QJsonDocument(ok.toObject()).toJson(QJsonDocument::Compact));
    return ok.toVariant().toString();
}

bool NiriCompositorService::applyEventForTest(const QJsonObject &event)
{
    return applyEvent(event);
}

void NiriCompositorService::requestRefresh()
{
    if (!active()) {
        if (m_socketPath.isEmpty())
            redetectSocketPath();
        if (!m_socketPath.isEmpty() && m_socket->state() == QLocalSocket::UnconnectedState)
            connectSocket();
        return;
    }

    sendRequest("{\"Workspaces\":null}\n", {QStringLiteral("Workspaces"), QString()});
    sendRequest("{\"Outputs\":null}\n", {QStringLiteral("Outputs"), QString()});
    sendRequest("{\"Windows\":null}\n", {QStringLiteral("Windows"), QString()});
    sendRequest("{\"FocusedWindow\":null}\n", {QStringLiteral("Focused"), QString()});
}

QVariantList NiriCompositorService::windowsForWorkspace(int workspaceIdx) const
{
    const QVariant workspaceId = workspaceIdForIdx(workspaceIdx);
    if (!workspaceId.isValid())
        return {};

    QVariantList result;
    for (const QVariant &window : m_windows) {
        const QVariantMap map = window.toMap();
        if (map.value(QStringLiteral("workspaceId")) == workspaceId)
            result.append(window);
    }
    return result;
}

void NiriCompositorService::focusWorkspaceIdx(int idx)
{
    if (idx <= 0)
        return;
    QJsonObject action;
    QJsonObject focus;
    QJsonObject ref;
    ref.insert(QStringLiteral("Index"), idx);
    focus.insert(QStringLiteral("reference"), ref);
    action.insert(QStringLiteral("FocusWorkspace"), focus);
    sendAction(action, QStringLiteral("focusWorkspace"));
    setFocusedWorkspaceIdx(idx);
    emit workspaceActivated(idx);
}

void NiriCompositorService::focusWorkspaceName(const QString &name)
{
    if (name.trimmed().isEmpty())
        return;
    QJsonObject action;
    QJsonObject focus;
    QJsonObject ref;
    ref.insert(QStringLiteral("Name"), name);
    focus.insert(QStringLiteral("reference"), ref);
    action.insert(QStringLiteral("FocusWorkspace"), focus);
    sendAction(action, QStringLiteral("focusWorkspace"));
}

void NiriCompositorService::focusWorkspaceRelative(int delta)
{
    if (delta == 0)
        return;

    const QVariantList indexes = sortedWorkspaceIndexes(m_workspaces);
    if (indexes.isEmpty())
        return;

    int currentIndex = -1;
    for (int index = 0; index < indexes.size(); ++index) {
        if (indexes.at(index).toInt() == m_focusedWorkspaceIdx) {
            currentIndex = index;
            break;
        }
    }

    if (currentIndex < 0)
        currentIndex = 0;

    const int nextIndex = (currentIndex + delta + indexes.size()) % indexes.size();
    focusWorkspaceIdx(indexes.at(nextIndex).toInt());
}

void NiriCompositorService::focusWindow(qulonglong windowId)
{
    if (windowId == 0)
        return;
    QJsonObject action;
    QJsonObject focus;
    focus.insert(QStringLiteral("id"), QJsonValue::fromVariant(windowId));
    action.insert(QStringLiteral("FocusWindow"), focus);
    sendAction(action, QStringLiteral("focusWindow"));
    setFocusedWindowId(windowId);
}

void NiriCompositorService::closeWindow(qulonglong windowId)
{
    if (windowId == 0)
        return;
    QJsonObject action;
    QJsonObject close;
    close.insert(QStringLiteral("id"), QJsonValue::fromVariant(windowId));
    action.insert(QStringLiteral("CloseWindow"), close);
    sendAction(action, QStringLiteral("closeWindow"));
}

void NiriCompositorService::closeFocusedWindow()
{
    closeWindow(m_focusedWindowId);
}

void NiriCompositorService::moveFocusedWindowToWorkspace(int workspaceIdx, bool focus)
{
    moveWindowToWorkspace(m_focusedWindowId, workspaceIdx, focus);
}

void NiriCompositorService::moveWindowToWorkspace(qulonglong windowId, int workspaceIdx, bool focus)
{
    if (windowId == 0 || workspaceIdx <= 0)
        return;
    QJsonObject action;
    QJsonObject move;
    move.insert(QStringLiteral("id"), QJsonValue::fromVariant(windowId));
    QJsonObject ref;
    ref.insert(QStringLiteral("Index"), workspaceIdx);
    move.insert(QStringLiteral("workspace"), ref);
    move.insert(QStringLiteral("focus"), focus);
    action.insert(QStringLiteral("MoveWindowToWorkspace"), move);
    sendAction(action, QStringLiteral("moveWindowToWorkspace"));
}

void NiriCompositorService::moveWindowToWorkspaceByName(qulonglong windowId, const QString &workspaceName, bool focus)
{
    if (windowId == 0 || workspaceName.trimmed().isEmpty())
        return;
    QJsonObject action;
    QJsonObject move;
    move.insert(QStringLiteral("id"), QJsonValue::fromVariant(windowId));
    QJsonObject ref;
    ref.insert(QStringLiteral("Name"), workspaceName);
    move.insert(QStringLiteral("workspace"), ref);
    move.insert(QStringLiteral("focus"), focus);
    action.insert(QStringLiteral("MoveWindowToWorkspace"), move);
    sendAction(action, QStringLiteral("moveWindowToWorkspace"));
}

void NiriCompositorService::handleSocketReadyRead()
{
    if (!m_socket)
        return;

    m_buffer.append(m_socket->readAll());
    while (true) {
        const int newline = m_buffer.indexOf('\n');
        if (newline < 0)
            break;

        const QByteArray rawLine = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (rawLine.trimmed().isEmpty())
            continue;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(rawLine, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qCWarning(lcNiri) << "Failed to parse niri IPC frame:" << error.errorString();
            continue;
        }

        const QJsonObject root = document.object();
        if (root.contains(QStringLiteral("Event"))) {
            applyEvent(root.value(QStringLiteral("Event")).toObject());
            continue;
        }

        if (m_pendingReplies.isEmpty()) {
            qCDebug(lcNiri) << "Received niri reply with no pending request:" << rawLine.left(120);
            continue;
        }

        processReply(m_pendingReplies.dequeue(), root);
    }
}

void NiriCompositorService::handleSocketDisconnected()
{
    setConnected(false);
    m_eventStreamOpen = false;
    m_buffer.clear();
    m_pendingReplies.clear();
    emit activeChanged();
    scheduleReconnect();
}

void NiriCompositorService::handleSocketError(QLocalSocket::LocalSocketError socketError)
{
    Q_UNUSED(socketError)
    setLastError(m_socket ? m_socket->errorString() : QStringLiteral("niri socket error"));
    setConnected(false);
    m_eventStreamOpen = false;
    emit activeChanged();
    scheduleReconnect();
}

void NiriCompositorService::handleReconnectTick()
{
    if (m_socketPath.isEmpty())
        redetectSocketPath();
    if (m_socketPath.isEmpty())
        return;
    if (m_socket && m_socket->state() != QLocalSocket::UnconnectedState)
        return;
    connectSocket();
}

void NiriCompositorService::connectSocket()
{
    if (m_socketPath.isEmpty() || !m_socket)
        return;
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        return;

    m_socket->connectToServer(m_socketPath);
    if (m_socket->waitForConnected(500)) {
        setConnected(true);
        startEventStream();
        return;
    }

    setLastError(m_socket->errorString());
    scheduleReconnect();
}

void NiriCompositorService::scheduleReconnect()
{
    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}

void NiriCompositorService::startEventStream()
{
    sendRequest("{\"EventStream\":null}\n", {QStringLiteral("EventStream"), QString()});
    sendRequest("{\"Workspaces\":null}\n", {QStringLiteral("Workspaces"), QString()});
    sendRequest("{\"Outputs\":null}\n", {QStringLiteral("Outputs"), QString()});
    sendRequest("{\"Windows\":null}\n", {QStringLiteral("Windows"), QString()});
    sendRequest("{\"FocusedWindow\":null}\n", {QStringLiteral("Focused"), QString()});
}

void NiriCompositorService::sendRequest(const QByteArray &payload, const PendingReply &pending)
{
    if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState) {
        if (!pending.actionName.isEmpty()) {
            const QString message = QStringLiteral("niri IPC is not connected");
            setLastError(message);
            emit actionFailed(pending.actionName, message);
        }
        return;
    }

    if (m_socket->write(payload) == -1) {
        const QString message = QStringLiteral("Failed to write to niri socket: %1").arg(m_socket->errorString());
        setLastError(message);
        if (!pending.actionName.isEmpty())
            emit actionFailed(pending.actionName, message);
        return;
    }

    m_socket->flush();
    m_pendingReplies.enqueue(pending);
}

void NiriCompositorService::sendAction(const QJsonObject &action, const QString &actionName)
{
    QJsonObject envelope;
    envelope.insert(QStringLiteral("Action"), action);
    sendRequest(QJsonDocument(envelope).toJson(QJsonDocument::Compact) + "\n",
                {QStringLiteral("Action"), actionName});
}

bool NiriCompositorService::applyEvent(const QJsonObject &event)
{
    const QStringList keys = event.keys();
    if (keys.isEmpty())
        return false;

    const QString name = keys.first();
    if (name == QStringLiteral("EventStreamOpened")) {
        m_eventStreamOpen = true;
        if (!m_pendingReplies.isEmpty() && m_pendingReplies.head().kind == QStringLiteral("EventStream"))
            m_pendingReplies.dequeue();
        emit activeChanged();
        QTimer::singleShot(kRefreshIntervalMs, this, &NiriCompositorService::refreshSnapshot);
        return true;
    }

    if (name == QStringLiteral("WorkspacesChanged")) {
        const QJsonObject payload = event.value(name).toObject();
        const QVariantList parsed = parseWorkspaces(payload.value(QStringLiteral("workspaces")).toArray());
        const int nextFocusedIdx = payload.value(QStringLiteral("focused_workspace_index")).toInt(m_focusedWorkspaceIdx);
        const bool workspacesUnchanged = parsed == m_workspaces;
        const bool focusedIdxChanged = nextFocusedIdx != m_focusedWorkspaceIdx;
        if (focusedIdxChanged)
            m_focusedWorkspaceIdx = nextFocusedIdx;
        setWorkspaces(parsed);
        if (workspacesUnchanged && focusedIdxChanged) {
            const QVariantMap nextFocused = resolveFocusedWorkspace();
            if (m_focusedWorkspace != nextFocused) {
                m_focusedWorkspace = nextFocused;
                emit focusedWorkspaceChanged();
            }
        }
        return true;
    }

    if (name == QStringLiteral("WorkspaceActiveWindowChanged")) {
        const QJsonObject payload = event.value(name).toObject();
        setFocusedWorkspaceIdx(payload.value(QStringLiteral("workspace_index")).toInt(m_focusedWorkspaceIdx));
        const QJsonValue activeId = payload.value(QStringLiteral("active_window_id"));
        setFocusedWindowId(activeId.isNull() ? 0 : activeId.toVariant().toULongLong());
        return true;
    }

    if (name == QStringLiteral("WorkspaceActivated")) {
        const QJsonObject payload = event.value(name).toObject();
        setFocusedWorkspaceIdx(payload.value(QStringLiteral("idx")).toInt(m_focusedWorkspaceIdx));
        return true;
    }

    if (name == QStringLiteral("WindowsChanged")) {
        const QJsonObject payload = event.value(name).toObject();
        setWindows(parseWindows(payload.value(QStringLiteral("windows")).toArray()));
        return true;
    }

    if (name == QStringLiteral("WindowFocusChanged") || name == QStringLiteral("FocusedWindowChanged")) {
        const QJsonObject payload = event.value(name).toObject();
        const QJsonValue id = payload.value(QStringLiteral("id"));
        setFocusedWindowId(id.isNull() ? 0 : id.toVariant().toULongLong());
        return true;
    }

    if (name == QStringLiteral("OutputsChanged")) {
        const QJsonObject payload = event.value(name).toObject();
        setOutputs(parseOutputs(payload.value(QStringLiteral("outputs")).toArray()));
        return true;
    }

    if (name == QStringLiteral("WindowClosed")) {
        const QJsonObject payload = event.value(name).toObject();
        const qulonglong closedId = payload.value(QStringLiteral("id")).toVariant().toULongLong();
        if (closedId == 0)
            return true;

        QVariantList next;
        next.reserve(m_windows.size());
        for (const QVariant &window : m_windows) {
            if (window.toMap().value(QStringLiteral("id")).toULongLong() != closedId)
                next.append(window);
        }
        setWindows(next);
        if (m_focusedWindowId == closedId)
            setFocusedWindowId(0);
        return true;
    }

    return false;
}

void NiriCompositorService::processReply(const PendingReply &pending, const QJsonObject &root)
{
    if (root.contains(QStringLiteral("Err"))) {
        const QString summary = replySummary(root, pending.kind);
        const QString message = summary.startsWith(QStringLiteral("error:")) ? summary.mid(6) : summary;
        setLastError(message);
        if (!pending.actionName.isEmpty())
            emit actionFailed(pending.actionName, message);
        return;
    }

    if (pending.kind == QStringLiteral("Action"))
        return;

    if (!root.contains(QStringLiteral("Ok")))
        return;

    const QJsonValue ok = root.value(QStringLiteral("Ok"));
    if (pending.kind == QStringLiteral("Focused")) {
        if (ok.isNull()) {
            setFocusedWindowId(0);
            return;
        }
        if (ok.isObject()) {
            const QVariantMap focused = parseWindowObject(ok.toObject());
            setFocusedWindowId(focused.value(QStringLiteral("id")).toULongLong());
            return;
        }
    }

    if (!ok.isArray())
        return;

    const QJsonArray array = ok.toArray();
    if (pending.kind == QStringLiteral("Workspaces")) {
        setWorkspaces(parseWorkspaces(array));
    } else if (pending.kind == QStringLiteral("Outputs")) {
        setOutputs(parseOutputs(array));
    } else if (pending.kind == QStringLiteral("Windows")) {
        setWindows(parseWindows(array));
    } else if (pending.kind == QStringLiteral("Focused")) {
        if (array.isEmpty()) {
            setFocusedWindowId(0);
            return;
        }
        const QVariantMap focused = parseWindows(array).value(0).toMap();
        setFocusedWindowId(focused.value(QStringLiteral("id")).toULongLong());
    }
}

void NiriCompositorService::refreshSnapshot()
{
    if (!active())
        return;
    requestRefresh();
    QTimer::singleShot(kRefreshIntervalMs, this, &NiriCompositorService::refreshSnapshot);
}

void NiriCompositorService::redetectSocketPath()
{
    const QString nextPath = detectSocketPath();
    if (m_socketPath == nextPath)
        return;
    m_socketPath = nextPath;
    emit socketPathChanged();
}

void NiriCompositorService::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

void NiriCompositorService::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void NiriCompositorService::setWorkspaces(const QVariantList &workspaces)
{
    if (m_workspaces == workspaces)
        return;
    m_workspaces = workspaces;
    emit workspacesChanged();

    const QVariantMap nextFocused = resolveFocusedWorkspace();
    if (m_focusedWorkspace != nextFocused) {
        m_focusedWorkspace = nextFocused;
        emit focusedWorkspaceChanged();
    }
}

void NiriCompositorService::setOutputs(const QVariantList &outputs)
{
    if (m_outputs == outputs)
        return;
    m_outputs = outputs;
    emit outputsChanged();
}

void NiriCompositorService::setWindows(const QVariantList &windows)
{
    if (m_windows == windows)
        return;
    m_windows = windows;
    emit windowsChanged();

    qulonglong nextFocusedId = m_focusedWindowId;
    for (const QVariant &window : m_windows) {
        const QVariantMap map = window.toMap();
        if (map.value(QStringLiteral("isFocused")).toBool()) {
            nextFocusedId = map.value(QStringLiteral("id")).toULongLong();
            break;
        }
    }
    setFocusedWindowId(nextFocusedId);
}

void NiriCompositorService::setFocusedWorkspaceIdx(int idx)
{
    if (idx <= 0 && m_focusedWorkspaceIdx == idx)
        return;

    const bool idxChanged = m_focusedWorkspaceIdx != idx;
    if (idxChanged)
        m_focusedWorkspaceIdx = idx;

    const QVariantMap nextFocused = resolveFocusedWorkspace();
    if (idxChanged || m_focusedWorkspace != nextFocused) {
        m_focusedWorkspace = nextFocused;
        emit focusedWorkspaceChanged();
    }
}

void NiriCompositorService::setFocusedWindowId(qulonglong windowId)
{
    const bool idChanged = m_focusedWindowId != windowId;
    if (idChanged)
        m_focusedWindowId = windowId;

    const QVariantMap nextFocused = resolveFocusedWindow();
    if (idChanged || m_focusedWindow != nextFocused) {
        m_focusedWindow = nextFocused;
        emit focusedWindowChanged();
    }
}

QVariantMap NiriCompositorService::resolveFocusedWorkspace() const
{
    QVariantMap fallback;
    for (const QVariant &workspace : m_workspaces) {
        const QVariantMap map = workspace.toMap();
        if (map.value(QStringLiteral("idx")).toInt() == m_focusedWorkspaceIdx)
            return map;
        if (fallback.isEmpty() && map.value(QStringLiteral("isFocused")).toBool())
            fallback = map;
    }
    return fallback;
}

QVariantMap NiriCompositorService::resolveFocusedWindow() const
{
    QVariantMap fallback;
    for (const QVariant &window : m_windows) {
        const QVariantMap map = window.toMap();
        if (map.value(QStringLiteral("id")).toULongLong() == m_focusedWindowId)
            return map;
        if (fallback.isEmpty() && map.value(QStringLiteral("isFocused")).toBool())
            fallback = map;
    }
    return fallback;
}

QVariant NiriCompositorService::workspaceIdForIdx(int workspaceIdx) const
{
    for (const QVariant &workspace : m_workspaces) {
        const QVariantMap map = workspace.toMap();
        if (map.value(QStringLiteral("idx")).toInt() == workspaceIdx)
            return map.value(QStringLiteral("id"));
    }
    return {};
}
