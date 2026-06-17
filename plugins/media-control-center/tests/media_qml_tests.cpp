#include <memory>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

class MediaQmlTests : public QObject {
    Q_OBJECT

private:
    QQmlEngine m_engine;

    QString sourcePath(const QString &relativePath) const
    {
        return QStringLiteral(MEDIA_PLUGIN_SOURCE_DIR) + QLatin1Char('/') + relativePath;
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
        qputenv("ARCHIPELAGO_MEDIA_DISABLE_DBUS_FOR_TESTS", "1");
        qputenv("ARCHIPELAGO_MEDIA_DISABLE_CAVA_FOR_TESTS", "1");
        m_engine.addImportPath(QStringLiteral(MEDIA_PLUGIN_BUILD_DIR));
        m_engine.addImportPath(sourcePath(QStringLiteral("tests/qml")));
    }

    void compactAndExpandedLoad()
    {
        const QList<QString> files = {
            QStringLiteral("ui/media/Compact.qml"),
            QStringLiteral("ui/media/Expanded.qml"),
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
        std::unique_ptr<QObject> object = create(QStringLiteral("ui/media/Compact.qml"), &error);
        QVERIFY2(object != nullptr, qPrintable(error));

        QVERIFY(object->property("preferredCompactWidth").isValid());
        QVERIFY(object->property("minimumCompactWidth").isValid());
        QVERIFY(object->property("maximumCompactWidth").isValid());
        QVERIFY(object->property("compactVisibleRequested").isValid());
        QVERIFY(object->property("compactLayoutPriority").isValid());

        QCOMPARE(object->property("compactVisibleRequested").toBool(), false);
        QVERIFY(object->property("minimumCompactWidth").toInt() >= 44);
        QVERIFY(object->property("maximumCompactWidth").toInt() >= object->property("minimumCompactWidth").toInt());
    }
};

QTEST_MAIN(MediaQmlTests)
#include "media_qml_tests.moc"
