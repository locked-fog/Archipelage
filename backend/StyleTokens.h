#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqml.h>

class StyleTokens final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(StyleTokens)
    QML_SINGLETON

    Q_PROPERTY(QColor transparent READ transparent CONSTANT FINAL)
    Q_PROPERTY(QColor black READ black CONSTANT FINAL)
    Q_PROPERTY(QColor white READ white CONSTANT FINAL)
    Q_PROPERTY(QColor panel READ panel CONSTANT FINAL)
    Q_PROPERTY(QColor module READ module CONSTANT FINAL)
    Q_PROPERTY(QColor moduleHover READ moduleHover CONSTANT FINAL)
    Q_PROPERTY(QColor track READ track CONSTANT FINAL)
    Q_PROPERTY(QColor textPrimary READ textPrimary CONSTANT FINAL)
    Q_PROPERTY(QColor textSecondary READ textSecondary CONSTANT FINAL)
    Q_PROPERTY(QColor textMuted READ textMuted CONSTANT FINAL)
    Q_PROPERTY(QColor accent READ accent CONSTANT FINAL)
    Q_PROPERTY(QColor success READ success CONSTANT FINAL)
    Q_PROPERTY(QColor warning READ warning CONSTANT FINAL)
    Q_PROPERTY(QColor danger READ danger CONSTANT FINAL)
    Q_PROPERTY(QColor overviewCard READ overviewCard CONSTANT FINAL)
    Q_PROPERTY(QColor overviewBorder READ overviewBorder CONSTANT FINAL)
    Q_PROPERTY(int radiusCapsule READ radiusCapsule CONSTANT FINAL)
    Q_PROPERTY(int radiusPanel READ radiusPanel CONSTANT FINAL)
    Q_PROPERTY(int radiusModule READ radiusModule CONSTANT FINAL)

public:
    explicit StyleTokens(QObject *parent = nullptr);

    QColor transparent() const;
    QColor black() const;
    QColor white() const;
    QColor panel() const;
    QColor module() const;
    QColor moduleHover() const;
    QColor track() const;
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor textMuted() const;
    QColor accent() const;
    QColor success() const;
    QColor warning() const;
    QColor danger() const;
    QColor overviewCard() const;
    QColor overviewBorder() const;
    int radiusCapsule() const;
    int radiusPanel() const;
    int radiusModule() const;
};
