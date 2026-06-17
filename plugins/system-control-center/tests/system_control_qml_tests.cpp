#include <memory>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

class SystemControlQmlTests : public QObject {
    Q_OBJECT

private:
    QQmlEngine m_engine;

    QString sourcePath(const QString &relativePath) const
    {
        return QStringLiteral(SYSTEM_CONTROL_PLUGIN_SOURCE_DIR) + QLatin1Char('/') + relativePath;
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
        qputenv("PATH", QByteArray("/nonexistent"));
        m_engine.addImportPath(QStringLiteral(SYSTEM_CONTROL_PLUGIN_BUILD_DIR));
        m_engine.addImportPath(sourcePath(QStringLiteral("tests/qml")));
    }

    void compactAndExpandedLoad()
    {
        const QList<QString> files = {
            QStringLiteral("ui/system-control/Compact.qml"),
            QStringLiteral("ui/system-control/Expanded.qml"),
            QStringLiteral("ui/system-control/WifiPreview.qml"),
            QStringLiteral("ui/system-control/BluetoothPreview.qml"),
            QStringLiteral("ui/system-control/AudioOutputPreview.qml"),
            QStringLiteral("ui/system-control/BluetoothDiscoveryPreview.qml"),
            QStringLiteral("ui/system-control/DisplayPreview.qml"),
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
        std::unique_ptr<QObject> object = create(QStringLiteral("ui/system-control/Compact.qml"), &error);
        QVERIFY2(object != nullptr, qPrintable(error));

        QVERIFY(object->property("preferredCompactWidth").isValid());
        QVERIFY(object->property("minimumCompactWidth").isValid());
        QVERIFY(object->property("maximumCompactWidth").isValid());
        QVERIFY(object->property("compactVisibleRequested").isValid());
        QVERIFY(object->property("compactLayoutPriority").isValid());
        QVERIFY(object->property("handlers").isValid());
    }

    void expandedHasVisibleControlRows()
    {
        QString error;
        std::unique_ptr<QObject> object = create(QStringLiteral("ui/system-control/Expanded.qml"), &error);
        QVERIFY2(object != nullptr, qPrintable(error));

        QVERIFY(object->property("controlRowCount").isValid());
        QCOMPARE(object->property("controlRowCount").toInt(), 4);

        QObject *powerCard = object->findChild<QObject *>(QStringLiteral("powerModeCard"));
        QVERIFY(powerCard != nullptr);
        QVERIFY(powerCard->property("height").toReal() > 70.0);
    }

    void secondaryPreviewHidesOriginControl()
    {
        QString error;
        std::unique_ptr<QObject> object = create(QStringLiteral("ui/system-control/Expanded.qml"), &error);
        QVERIFY2(object != nullptr, qPrintable(error));

        const QList<QObject *> quickCards = object->findChildren<QObject *>(
            QRegularExpression(QStringLiteral("^quickSettingCard:")));
        QVERIFY(quickCards.size() >= 2);

        QObject *wifiCard = nullptr;
        for (QObject *candidate : quickCards) {
            if (candidate && candidate->property("title").toString() == QStringLiteral("Wi-Fi")) {
                wifiCard = candidate;
                break;
            }
        }
        QVERIFY(wifiCard != nullptr);

        QVERIFY(object->setProperty("activeSecondaryPreview", QStringLiteral("wifi")));
        QTRY_VERIFY(wifiCard->property("opacity").toReal() < 0.1);
        QVERIFY(wifiCard->property("visible").toBool());

        QObject *audioButton = object->findChild<QObject *>(QStringLiteral("mixerButton"));
        QVERIFY(audioButton != nullptr);

        QVERIFY(object->setProperty("activeSecondaryPreview", QStringLiteral("audio-output")));
        QTRY_VERIFY(audioButton->property("opacity").toReal() < 0.1);
        QVERIFY(audioButton->property("visible").toBool());

        QVERIFY(QMetaObject::invokeMethod(object.get(), "dismissActivePreview"));
        QCOMPARE(object->property("activeSecondaryPreview").toString(), QString());
    }
};

QTEST_MAIN(SystemControlQmlTests)
#include "system_control_qml_tests.moc"
