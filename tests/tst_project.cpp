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
    void spectrumConsumersAreDiscovered();
    void mirrorSpectrumIsSymmetric();
    void v1ProjectsMigrateToProvider();
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
    // Checked precisely rather than by searching for "audio", which is also the
    // audio provider layer's type name.
    QVERIFY(!contents.contains("trackPath"));
    QVERIFY(!contents.contains("audioPath"));
    for (const char *ext : {".mp3", ".wav", ".flac", ".m4a", ".aac", ".ogg"})
        QVERIFY2(!contents.contains(ext), ext);
    // No absolute path of any kind should have been written.
    QVERIFY(!contents.contains(":/") || !contents.contains(":\\"));

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
    model.addLayer(QStringLiteral("particles"));
    model.addLayer(QStringLiteral("bars"));
    model.addLayer(QStringLiteral("text"));

    // Give the particle layer non-default values too, so its properties are
    // covered by the round-trip and not just its type.
    auto *particles = qobject_cast<ParticleLayer *>(model.at(1));
    QVERIFY(particles);
    particles->setProperty("band", QStringLiteral("treble"));
    particles->setProperty("reactiveRate", 777.0);
    particles->setProperty("color", QColor(QStringLiteral("#12ab34")));

    QVERIFY(project.save(QUrl::fromLocalFile(path)));

    LayerModel loaded;
    Project loadProject;
    loadProject.setLayerModel(&loaded);
    QVERIFY(loadProject.load(QUrl::fromLocalFile(path)));

    QCOMPARE(loaded.count(), 4);
    QCOMPARE(loaded.at(0)->typeId(), QStringLiteral("background"));
    QCOMPARE(loaded.at(1)->typeId(), QStringLiteral("particles"));
    QCOMPARE(loaded.at(2)->typeId(), QStringLiteral("bars"));
    QCOMPARE(loaded.at(3)->typeId(), QStringLiteral("text"));

    QCOMPARE(loaded.at(1)->property("band").toString(), QStringLiteral("treble"));
    QCOMPARE(loaded.at(1)->property("reactiveRate").toDouble(), 777.0);
    QCOMPARE(loaded.at(1)->property("color").value<QColor>(), QColor(QStringLiteral("#12ab34")));
}

void TestProject::spectrumConsumersAreDiscovered()
{
    LayerModel model;
    model.clear();

    model.addLayer(QStringLiteral("audio"));
    model.addLayer(QStringLiteral("background"));
    model.addLayer(QStringLiteral("bars"));
    model.addLayer(QStringLiteral("text"));
    model.addLayer(QStringLiteral("particles"));

    // Since v2 the FFT is run for providers and particle layers only. Bar layers
    // are fed from a provider, so adding ten of them costs no extra analysis.
    const QVector<Layer *> consumers = model.spectrumLayers();
    QCOMPARE(consumers.size(), 2);
    QCOMPARE(consumers.at(0)->typeId(), QStringLiteral("audio"));
    QCOMPARE(consumers.at(1)->typeId(), QStringLiteral("particles"));

    QCOMPARE(model.audioProviders().size(), 1);
    QCOMPARE(model.barLayers().size(), 1);
    QVERIFY(!model.barLayers().first()->needsSpectrum());

    // The provider conditions data but draws nothing.
    QVERIFY(!model.audioProviders().first()->isVisual());
    QVERIFY(model.barLayers().first()->isVisual());

    // Each consumer analyzes under its own settings, keyed by a unique id.
    QVERIFY(consumers.at(0)->layerId() != consumers.at(1)->layerId());
}

void TestProject::mirrorSpectrumIsSymmetric()
{
    LayerModel model;
    model.clear();
    model.addLayer(QStringLiteral("bars"));
    auto *bars = qobject_cast<BarLayer *>(model.at(0));
    QVERIFY(bars);

    SpectrumFrame frame;
    frame.bands = {0.1f, 0.2f, 0.3f, 0.4f};

    // Values originate as float and widen to qreal, so compare with tolerance
    // rather than exactly.
    const auto sameAs = [](const QList<qreal> &got, const QList<qreal> &want) {
        if (got.size() != want.size())
            return false;
        for (int i = 0; i < got.size(); ++i) {
            if (std::abs(got.at(i) - want.at(i)) > 1e-6)
                return false;
        }
        return true;
    };

    bars->setProperty("mirrorSpectrum", false);
    bars->takeFrame(frame);
    QCOMPARE(bars->barCount(), 4);
    QVERIFY(sameAs(bars->bands(), {0.1, 0.2, 0.3, 0.4}));

    // Mirroring doubles the bar count and puts the LOWEST band at the centre.
    bars->setProperty("mirrorSpectrum", true);
    bars->takeFrame(frame);
    QCOMPARE(bars->barCount(), 8);
    const QList<qreal> b = bars->bands();
    QVERIFY(sameAs(b, {0.4, 0.3, 0.2, 0.1, 0.1, 0.2, 0.3, 0.4}));

    // Exactly symmetric about the centre.
    for (int i = 0; i < b.size(); ++i)
        QCOMPARE(b.at(i), b.at(b.size() - 1 - i));
}

void TestProject::v1ProjectsMigrateToProvider()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("legacy.mvproj"));

    // A v1 file: analysis settings live on the bar layer, no provider exists.
    QJsonObject barProps;
    barProps.insert(QStringLiteral("name"), QStringLiteral("Old bars"));
    barProps.insert(QStringLiteral("barCount"), 72);
    barProps.insert(QStringLiteral("fftSize"), 4096);
    barProps.insert(QStringLiteral("minHz"), 45.0);
    barProps.insert(QStringLiteral("maxHz"), 15000.0);
    barProps.insert(QStringLiteral("attackMs"), 12.0);
    barProps.insert(QStringLiteral("decayMs"), 260.0);
    barProps.insert(QStringLiteral("colorMode"), QStringLiteral("spectrum"));

    QJsonObject bar;
    bar.insert(QStringLiteral("type"), QStringLiteral("bars"));
    bar.insert(QStringLiteral("props"), barProps);

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("music-visualizer-project"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("layers"), QJsonArray{bar});

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    LayerModel model;
    Project project;
    project.setLayerModel(&model);
    QVERIFY2(project.load(QUrl::fromLocalFile(path)), qPrintable(project.lastError()));

    // A provider is synthesised, carrying the old analysis settings.
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.at(0)->typeId(), QStringLiteral("audio"));
    auto *provider = qobject_cast<AudioProviderLayer *>(model.at(0));
    QVERIFY(provider);
    QCOMPARE(provider->property("bandCount").toInt(), 72);
    QCOMPARE(provider->property("fftSize").toInt(), 4096);
    QCOMPARE(provider->property("minHz").toDouble(), 45.0);
    QCOMPARE(provider->property("maxHz").toDouble(), 15000.0);
    QCOMPARE(provider->property("attackMs").toDouble(), 12.0);
    QCOMPARE(provider->property("decayMs").toDouble(), 260.0);

    // The bar layer keeps its visual settings and loses the analysis ones.
    QCOMPARE(model.at(1)->typeId(), QStringLiteral("bars"));
    QCOMPARE(model.at(1)->property("colorMode").toString(), QStringLiteral("spectrum"));
    QCOMPARE(model.at(1)->property("name").toString(), QStringLiteral("Old bars"));
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
