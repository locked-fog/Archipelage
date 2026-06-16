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

private slots:
    void initTestCase()
    {
        qputenv("ARCHIPELAGO_MEDIA_DISABLE_DBUS_FOR_TESTS", "1");
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
            QQmlComponent component(&m_engine, sourceUrl(file));
            if (component.isLoading()) {
                QSignalSpy spy(&component, &QQmlComponent::statusChanged);
                QVERIFY(spy.wait(1000));
            }
            if (component.isError())
                qWarning().noquote() << component.errorString();
            QCOMPARE(component.status(), QQmlComponent::Ready);

            std::unique_ptr<QObject> object(component.create());
            QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        }
    }
};

QTEST_MAIN(MediaQmlTests)
#include "media_qml_tests.moc"
