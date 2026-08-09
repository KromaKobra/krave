#include "LayerModel.h"
#include "Project.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace mv;

class TestProject : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripPreservesEveryProperty();
    void projectFileContainsNoAudioPath();
    void rejectsNewerFormatVersion();
    void layerOrderIsPreserved();
    void modelReordersAndDeletes();
};

void TestProject::roundTripPreservesEveryProperty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("round-trip.mvproj"));

    LayerModel saveModel;
    Project saveProject;
    saveProject.setLayerModel(&saveModel);
    saveProject.newProject();

    const int index = saveModel.addLayer(QStringLiteral("bars"));
    QVERIFY(index >= 0);
    auto *bars = qobject_cast<BarLayer *>(saveModel.at(index));
    QVERIFY(bars);

    // Push every property off its default so a missed field cannot pass by luck.
    bars->setProperty("name", QStringLiteral("Custom bars"));
    bars->setProperty("barCount", 111);
    bars->setProperty("barWidthRatio", 0.42);
    bars->setProperty("cornerRadius", 7.5);
    bars->setProperty("minHeight", 0.02);
    bars->setProperty("maxHeight", 0.66);
    bars->setProperty("orientation", QStringLiteral("radial"));
    bars->setProperty("innerRadius", 0.51);
    bars->setProperty("growUpward", false);
    bars->setProperty("colorMode", QStringLiteral("spectrum"));
    bars->setProperty("colorA", QColor(QStringLiteral("#ff8800")));
    bars->setProperty("colorB", QColor(QStringLiteral("#0088ff")));
    bars->setProperty("reactiveBrightness", 1.25);
    bars->setProperty("fftSize", 4096);
    bars->setProperty("minHz", 45.0);
    bars->setProperty("maxHz", 14000.0);
    bars->setProperty("floorDb", -80.0);
    bars->setProperty("ceilingDb", -3.0);
    bars->setProperty("scalingCurve", QStringLiteral("power"));
    bars->setProperty("gamma", 2.5);
    bars->setProperty("attackMs", 33.0);
    bars->setProperty("decayMs", 275.0);
    bars->setProperty("layerOpacity", 0.72);
    bars->setProperty("posX", 0.31);
    bars->setProperty("posY", 0.62);
    bars->setProperty("sizeW", 0.83);
    bars->setProperty("sizeH", 0.44);
    bars->setProperty("layerRotation", 22.0);
    bars->setProperty("layerVisible", false);

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));
    QVERIFY(!saveProject.isModified());

    LayerModel loadModel;
    Project loadProject;
    loadProject.setLayerModel(&loadModel);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)),
             qPrintable(loadProject.lastError()));
    QVERIFY(!loadProject.isModified());

    QCOMPARE(loadModel.count(), saveModel.count());

    auto *reloaded = qobject_cast<BarLayer *>(loadModel.at(index));
    QVERIFY(reloaded);

    // Compare by walking the meta-object, so a property added later is covered
    // by this test automatically.
    const QMetaObject *meta = bars->metaObject();
    for (int i = Layer::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        if (!prop.isWritable() || !prop.isStored())
            continue;
        QVERIFY2(prop.read(reloaded) == prop.read(bars),
                 qPrintable(QStringLiteral("property '%1' differs: %2 vs %3")
                                .arg(QString::fromUtf8(prop.name()),
                                     prop.read(reloaded).toString(),
                                     prop.read(bars).toString())));
    }
}

void TestProject::projectFileContainsNoAudioPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("no-audio.mvproj"));

    LayerModel model;
    Project project;
    project.setLayerModel(&model);
    project.newProject();
    model.addLayer(QStringLiteral("bars"));
    QVERIFY(project.save(QUrl::fromLocalFile(path)));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();

    // Projects are meant to be reusable templates: no track reference at all.
    QVERIFY(!contents.contains("audio"));
    QVERIFY(!contents.contains(".mp3"));
    QVERIFY(!contents.contains(".wav"));
    QVERIFY(!contents.contains("trackPath"));

    const QJsonObject root = QJsonDocument::fromJson(contents).object();
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), Project::kFormatVersion);
    QVERIFY(root.contains(QStringLiteral("layers")));
    QVERIFY(root.contains(QStringLiteral("canvas")));
}

void TestProject::rejectsNewerFormatVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("future.mvproj"));

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("music-visualizer-project"));
    root.insert(QStringLiteral("version"), Project::kFormatVersion + 5);
    root.insert(QStringLiteral("layers"), QJsonArray());

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    LayerModel model;
    Project project;
    project.setLayerModel(&model);

    QVERIFY(!project.load(QUrl::fromLocalFile(path)));
    QVERIFY(!project.lastError().isEmpty());
}

void TestProject::layerOrderIsPreserved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("order.mvproj"));

    LayerModel model;
    Project project;
    project.setLayerModel(&model);
    project.newProject();
    model.clear();

    model.addLayer(QStringLiteral("background"));
    model.addLayer(QStringLiteral("bars"));
    model.addLayer(QStringLiteral("text"));
    QVERIFY(project.save(QUrl::fromLocalFile(path)));

    LayerModel loaded;
    Project loadProject;
    loadProject.setLayerModel(&loaded);
    QVERIFY(loadProject.load(QUrl::fromLocalFile(path)));

    QCOMPARE(loaded.count(), 3);
    QCOMPARE(loaded.at(0)->typeId(), QStringLiteral("background"));
    QCOMPARE(loaded.at(1)->typeId(), QStringLiteral("bars"));
    QCOMPARE(loaded.at(2)->typeId(), QStringLiteral("text"));
}

void TestProject::modelReordersAndDeletes()
{
    LayerModel model;
    model.clear();

    model.addLayer(QStringLiteral("background"));
    model.addLayer(QStringLiteral("bars"));
    model.addLayer(QStringLiteral("text"));
    QCOMPARE(model.count(), 3);

    // Move the text layer to the bottom of the stack.
    model.moveLayer(2, 0);
    QCOMPARE(model.at(0)->typeId(), QStringLiteral("text"));
    QCOMPARE(model.at(1)->typeId(), QStringLiteral("background"));
    QCOMPARE(model.at(2)->typeId(), QStringLiteral("bars"));

    model.removeLayer(1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.at(0)->typeId(), QStringLiteral("text"));
    QCOMPARE(model.at(1)->typeId(), QStringLiteral("bars"));

    // Layer ids must stay unique so the spectrum engine cannot cross wires.
    QVERIFY(model.at(0)->layerId() != model.at(1)->layerId());

    model.removeLayer(99); // out of range, must be a no-op
    QCOMPARE(model.count(), 2);
}

QTEST_MAIN(TestProject)
#include "tst_project.moc"
