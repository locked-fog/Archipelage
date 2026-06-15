#include "CompositorProvider.h"

#include "NiriCompositorService.h"

#include <QLoggingCategory>
#include <QtGlobal>

namespace {
Q_LOGGING_CATEGORY(lcCompositor, "archipelago.compositor")
}

CompositorProvider::CompositorProvider(QObject *parent)
    : QObject(parent)
{
    const QString configured = qEnvironmentVariable("ARCHIPELAGO_COMPOSITOR", "auto");
    m_compositorName = detectCompositor(configured);
    qCInfo(lcCompositor) << "Detected compositor:" << m_compositorName;

    if (m_compositorName == QStringLiteral("niri")) {
        m_niriService = new NiriCompositorService(this);
        wireNiriService();
    }

    resolveActiveFlag();
}

CompositorProvider::~CompositorProvider() = default;

QString CompositorProvider::detectCompositor(const QString &configuredValue)
{
    const QString normalized = configuredValue.trimmed().toLower();

    if (normalized == QStringLiteral("none"))
        return QStringLiteral("none");
    if (normalized == QStringLiteral("niri"))
        return NiriCompositorService::detectSocketPath().isEmpty() ? QStringLiteral("none") : QStringLiteral("niri");
    if (normalized == QStringLiteral("hyprland"))
        return qEnvironmentVariableIsSet("HYPRLAND_INSTANCE_SIGNATURE") ? QStringLiteral("hyprland") : QStringLiteral("none");

    if (!NiriCompositorService::detectSocketPath().isEmpty())
        return QStringLiteral("niri");
    if (qEnvironmentVariableIsSet("HYPRLAND_INSTANCE_SIGNATURE"))
        return QStringLiteral("hyprland");
    return QStringLiteral("none");
}

QString CompositorProvider::compositorName() const { return m_compositorName; }
bool CompositorProvider::active() const { return m_active; }
QString CompositorProvider::lastError() const { return m_lastError; }
QObject *CompositorProvider::niriService() const { return m_niriService; }

void CompositorProvider::resolveActiveFlag()
{
    const bool nextActive = m_compositorName == QStringLiteral("hyprland")
        || (m_compositorName == QStringLiteral("niri") && m_niriService && m_niriService->active());

    if (m_active == nextActive)
        return;
    m_active = nextActive;
    emit activeChanged();
}

void CompositorProvider::wireNiriService()
{
    if (!m_niriService)
        return;

    connect(m_niriService, &NiriCompositorService::activeChanged, this, [this]() {
        resolveActiveFlag();
    });
    connect(m_niriService, &NiriCompositorService::lastErrorChanged, this, [this]() {
        if (!m_niriService)
            return;
        m_lastError = m_niriService->lastError();
        emit lastErrorChanged();
    });
}
