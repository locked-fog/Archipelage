#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

class NiriCompositorService;

class CompositorProvider final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Compositor)
    QML_SINGLETON

    Q_PROPERTY(QString compositorName READ compositorName NOTIFY compositorNameChanged FINAL)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QObject *niriService READ niriService CONSTANT FINAL)

public:
    explicit CompositorProvider(QObject *parent = nullptr);
    ~CompositorProvider() override;

    static QString detectCompositor(const QString &configuredValue);

    QString compositorName() const;
    bool active() const;
    QString lastError() const;
    QObject *niriService() const;

signals:
    void compositorNameChanged();
    void activeChanged();
    void lastErrorChanged();

private:
    void resolveActiveFlag();
    void wireNiriService();

    QString m_compositorName;
    QString m_lastError;
    NiriCompositorService *m_niriService = nullptr;
    bool m_active = false;
};
