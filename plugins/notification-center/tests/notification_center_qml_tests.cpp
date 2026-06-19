#include <memory>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

class NotificationCenterQmlTests : public QObject {
    Q_OBJECT

private:
    QQmlEngine m_engine;

    QString sourcePath(const QString &relativePath) const
    {
        return QStringLiteral(NOTIFICATION_CENTER_PLUGIN_SOURCE_DIR) + QLatin1Char('/') + relativePath;
    }

    QUrl sourceUrl(const QString &relativePath) const
    {
        return QUrl::fromLocalFile(sourcePath(relativePath));
    }

    std::unique_ptr<QObject> create(const QString &relativePath, QString *errorOut = nullptr)
    {
        QQmlComponent component(&m_engine, sourceUrl(relativePath));
        if (component.isLoading()) {
            QSignalSpy spy(&component, &QQmlComponent::statusChanged);
            if (!spy.wait(1000))
                return {};
        }
        if (component.isError())
            qWarning().noquote() << component.errorString();

        if (errorOut)
            *errorOut = component.errorString();
        return std::unique_ptr<QObject>(component.create());
    }

private slots:
    void initTestCase()
    {
        qputenv("ARCHIPELAGO_NOTIFICATIONS_DISABLE_DBUS_FOR_TESTS", "1");
        m_engine.addImportPath(QStringLiteral(NOTIFICATION_CENTER_PLUGIN_BUILD_DIR));
        m_engine.addImportPath(sourcePath(QStringLiteral("tests/qml")));
    }

    void compactExpandedPreviewAndGlyphLoad()
    {
        const QList<QString> files = {
            QStringLiteral("ui/notifications/Compact.qml"),
            QStringLiteral("ui/notifications/Expanded.qml"),
            QStringLiteral("ui/notifications/NotificationPreview.qml"),
            QStringLiteral("ui/notifications/NotificationAvatar.qml"),
            QStringLiteral("ui/notifications/NotificationGlyph.qml"),
        };

        for (const QString &file : files) {
            QString error;
            std::unique_ptr<QObject> object = create(file, &error);
            QVERIFY2(object != nullptr, qPrintable(error));
        }
    }

    void compactExposesDynamicLayoutProperties()
    {
        QString error;
        std::unique_ptr<QObject> object = create(QStringLiteral("ui/notifications/Compact.qml"), &error);
        QVERIFY2(object != nullptr, qPrintable(error));

        QVERIFY(object->property("preferredCompactWidth").isValid());
        QVERIFY(object->property("minimumCompactWidth").isValid());
        QVERIFY(object->property("maximumCompactWidth").isValid());
        QVERIFY(object->property("compactVisibleRequested").isValid());
        QVERIFY(object->property("compactLayoutPriority").isValid());
        QVERIFY(object->property("handlers").isValid());
        QVERIFY(object->property("preferredCompactWidth").toInt() >= object->property("minimumCompactWidth").toInt());
    }

    void expandedExposesHeaderButtons()
    {
        QString error;
        std::unique_ptr<QObject> object = create(QStringLiteral("ui/notifications/Expanded.qml"), &error);
        QVERIFY2(object != nullptr, qPrintable(error));

        QVERIFY(object->findChild<QObject *>(QStringLiteral("clearButton")) != nullptr);
        QVERIFY(object->findChild<QObject *>(QStringLiteral("modeButton")) != nullptr);
    }
};

QTEST_MAIN(NotificationCenterQmlTests)
#include "notification_center_qml_tests.moc"
