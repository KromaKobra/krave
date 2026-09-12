#include "NodeGraph.h"
#include "Project.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace mv;

namespace {

/// The v3 node object shape, so the hand-written fixtures below stay readable.
QJsonObject jsonNode(int id, const QString &type, const QJsonObject &props = {})
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("type"), type);
    obj.insert(QStringLiteral("props"), props);
    return obj;
}

QJsonObject jsonEdge(int from, int to, const QString &port)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("from"), from);
    obj.insert(QStringLiteral("to"), to);
    obj.insert(QStringLiteral("port"), port);
    return obj;
}

bool writeProject(const QString &path, int version, const QJsonArray &nodes,
                  const QJsonArray &connections)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("music-visualizer-project"));
    root.insert(QStringLiteral("version"), version);
    root.insert(QStringLiteral("nodes"), nodes);
    root.insert(QStringLiteral("connections"), connections);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson());
    return true;
}

/// A valid but otherwise empty graph: just the two essential nodes.
///
/// A default-constructed NodeGraph has no Output at all, which is a state no
/// public path in the app produces -- newProject and load both make one. Tests
/// that want a bare canvas to build on start here rather than from nothing.
void minimalGraph(NodeGraph &graph)
{
    graph.resetNodes({NodeGraph::createNode(QStringLiteral("output"), 1),
                      NodeGraph::createNode(QStringLiteral("audio"), 2)},
                     {});
}

/// The set of edges in a graph, as "from>to.port" strings, for comparing by value.
QSet<QString> edgeSet(const NodeGraph &graph)
{
    QSet<QString> result;
    for (const Node *to : graph.nodes()) {
        const QVector<PortSpec> &ports = to->spec().inputs;
        for (int port = 0; port < ports.size(); ++port) {
            if (const Node *from = to->input(port)) {
                result.insert(QStringLiteral("%1>%2.%3")
                                  .arg(from->nodeId())
                                  .arg(to->nodeId())
                                  .arg(ports.at(port).name));
            }
        }
    }
    return result;
}

} // namespace

class TestProject : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripPreservesEveryProperty();
    void projectFileContainsNoAudioPath();
    void rejectsNewerFormatVersion();
    void rejectsV2Files();
    void renderListFollowsTheVisualChain();
    void audioSourcesAreDiscovered();
    void mirrorSpectrumIsSymmetric();
    void nodeIdsAreStableAcrossSaveAndLoad();
    void graphPositionsRoundTrip();
    void connectionsRoundTripByPortName();
    void imageNodesRoundTripTheirSourceAndCrop();
    void transformNodesRoundTripTheirPlacement();
    void blurNodesRoundTripEverySetting();
    void aMergesTwoEdgesSurviveAsDistinctPorts();
    void loadingAFileWithoutAnOutputSynthesizesOne();
    void loadingAFileWithAnInvalidConnectionDropsOnlyThatEdge();
    void everyExampleProjectLoads();
};

void TestProject::roundTripPreservesEveryProperty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("round-trip.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    const int id = saveGraph.addNode(QStringLiteral("spectrum"), 0, 0);
    QVERIFY(id > 0);
    auto *bars = qobject_cast<SpectrumNode *>(saveGraph.nodeById(id));
    QVERIFY(bars);

    // Push every property off its default so a missed field cannot pass by luck.
    bars->setProperty("name", QStringLiteral("Custom bars"));
    bars->setProperty("barWidthRatio", 0.42);
    bars->setProperty("cornerRadius", 7.5);
    bars->setProperty("minHeight", 0.02);
    bars->setProperty("maxHeight", 0.66);
    bars->setProperty("orientation", QStringLiteral("radial"));
    bars->setProperty("mirrorBars", true);
    bars->setProperty("mirrorSpectrum", true);
    bars->setProperty("bassInner", true);
    bars->setProperty("innerRadius", 0.51);
    bars->setProperty("arc", -0.37);
    bars->setProperty("barsMatchArc", true);
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
    // graphX/graphY are covered by the meta-walk below for free, which is the
    // point of declaring them as properties on Node rather than as plain members.
    bars->setProperty("graphX", 412.0);
    bars->setProperty("graphY", -96.5);

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));
    QVERIFY(!saveProject.isModified());

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));
    QVERIFY(!loadProject.isModified());

    QCOMPARE(loadGraph.count(), saveGraph.count());

    auto *reloaded = qobject_cast<SpectrumNode *>(loadGraph.nodeById(id));
    QVERIFY(reloaded);

    // Compare by walking the meta-object, so a property added later is covered
    // by this test automatically.
    const QMetaObject *meta = bars->metaObject();
    for (int i = Node::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        if (!prop.isWritable() || !prop.isStored())
            continue;
        QVERIFY2(prop.read(reloaded) == prop.read(bars),
                 qPrintable(QStringLiteral("property '%1' differs: %2 vs %3")
                                .arg(QString::fromUtf8(prop.name()),
                                     prop.read(reloaded).toString(),
                                     prop.read(bars).toString())));
    }

    // The edge set is part of the document too, and nothing in the meta-walk
    // covers it.
    QCOMPARE(edgeSet(loadGraph), edgeSet(saveGraph));
}

void TestProject::projectFileContainsNoAudioPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("no-audio.mvproj"));

    NodeGraph graph;
    Project project;
    project.setGraph(&graph);
    project.newProject();
    graph.addNode(QStringLiteral("spectrum"), 0, 0);
    QVERIFY(project.save(QUrl::fromLocalFile(path)));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();

    // Projects are meant to be reusable templates: no track reference at all.
    // Checked precisely rather than by searching for "audio", which is also the
    // audio node's type name.
    QVERIFY(!contents.contains("trackPath"));
    QVERIFY(!contents.contains("audioPath"));
    for (const char *ext : {".mp3", ".wav", ".flac", ".m4a", ".aac", ".ogg"})
        QVERIFY2(!contents.contains(ext), ext);
    // No absolute path of any kind should have been written.
    QVERIFY(!contents.contains(":/") || !contents.contains(":\\"));

    const QJsonObject root = QJsonDocument::fromJson(contents).object();
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), Project::kFormatVersion);
    QVERIFY(root.contains(QStringLiteral("nodes")));
    QVERIFY(root.contains(QStringLiteral("connections")));
    QVERIFY(root.contains(QStringLiteral("canvas")));
    // The stack is gone; a file still claiming one would be a half-done migration.
    QVERIFY(!root.contains(QStringLiteral("layers")));
}

void TestProject::rejectsNewerFormatVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("future.mvproj"));
    QVERIFY(writeProject(path, Project::kFormatVersion + 5, {}, {}));

    NodeGraph graph;
    Project project;
    project.setGraph(&graph);

    QVERIFY(!project.load(QUrl::fromLocalFile(path)));
    QVERIFY(!project.lastError().isEmpty());
}

void TestProject::rejectsV2Files()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("legacy.mvproj"));

    // A v2 file: an ordered "layers" array, no connections, typeId "bars".
    QJsonObject bar;
    bar.insert(QStringLiteral("type"), QStringLiteral("bars"));
    bar.insert(QStringLiteral("props"), QJsonObject());

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("music-visualizer-project"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("layers"), QJsonArray{bar});

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    NodeGraph graph;
    Project project;
    project.setGraph(&graph);

    QVERIFY(!project.load(QUrl::fromLocalFile(path)));
    // The error has to name the way out, or the user is simply stuck.
    QVERIFY2(project.lastError().contains(QStringLiteral("migrate_v2_to_v3")),
             qPrintable(project.lastError()));
}

void TestProject::renderListFollowsTheVisualChain()
{
    NodeGraph graph;
    minimalGraph(graph);

    // background -> text -> spectrum -> output. Built in an order that is NOT the
    // draw order, so the list can only be right by following the edges.
    const int spectrum = graph.addNode(QStringLiteral("spectrum"), 0, 0);
    const int background = graph.addNode(QStringLiteral("background"), 0, 0);
    const int text = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(spectrum > 0 && background > 0 && text > 0);

    OutputNode *output = graph.outputNode();
    QVERIFY(output);

    QVERIFY(graph.link(background, text, QStringLiteral("visual")));
    QVERIFY(graph.link(text, spectrum, QStringLiteral("visual")));
    QVERIFY(graph.link(spectrum, output->nodeId(), QStringLiteral("visual")));

    const QVector<VisualNode *> drawn = graph.renderList()->nodes();
    QCOMPARE(drawn.size(), 3);
    QCOMPARE(drawn.at(0)->nodeId(), background);
    QCOMPARE(drawn.at(1)->nodeId(), text);
    QCOMPARE(drawn.at(2)->nodeId(), spectrum);

    // The output is the root of the render, not a thing that gets rendered.
    for (const VisualNode *node : drawn)
        QVERIFY(node->nodeId() != output->nodeId());
    QVERIFY(!qobject_cast<VisualNode *>(output));
}

void TestProject::audioSourcesAreDiscovered()
{
    NodeGraph graph;
    graph.loadDefaults();

    // The default project is audio -> separator -> spectrum, so both sources are
    // live and the spectrum is not one of them: it is fed a frame, it does not
    // run an FFT. Adding ten spectrum nodes costs no extra analysis.
    const QVector<AudioSourceNode *> sources = graph.liveAudioSources();
    QCOMPARE(sources.size(), 2);

    QStringList types;
    QSet<int> ids;
    for (AudioSourceNode *source : sources) {
        types.append(source->typeId());
        // Ids key the analyzer map in SpectrumWorker, so uniqueness is much more
        // load-bearing than it looks.
        QVERIFY(!ids.contains(source->nodeId()));
        ids.insert(source->nodeId());
    }
    types.sort();
    QCOMPARE(types, QStringList({QStringLiteral("audio"), QStringLiteral("separator")}));

    QCOMPARE(graph.spectrumNodes().size(), 1);
    for (SpectrumNode *spectrum : graph.spectrumNodes())
        QVERIFY(!qobject_cast<AudioSourceNode *>(spectrum));

    // The separator conditions data but draws nothing; the spectrum draws.
    QVERIFY(graph.audioNode());
    QVERIFY(!graph.audioNode()->isVisual());
    QVERIFY(graph.spectrumNodes().first()->isVisual());
}

void TestProject::mirrorSpectrumIsSymmetric()
{
    NodeGraph graph;
    minimalGraph(graph);
    const int id = graph.addNode(QStringLiteral("spectrum"), 0, 0);
    auto *bars = qobject_cast<SpectrumNode *>(graph.nodeById(id));
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

    // Mirroring doubles the bar count and puts the LOWEST band at both outer
    // edges, with the highest bands meeting in the centre.
    bars->setProperty("mirrorSpectrum", true);
    bars->takeFrame(frame);
    QCOMPARE(bars->barCount(), 8);
    const QList<qreal> b = bars->bands();
    QVERIFY(sameAs(b, {0.1, 0.2, 0.3, 0.4, 0.4, 0.3, 0.2, 0.1}));

    // Exactly symmetric about the centre.
    for (int i = 0; i < b.size(); ++i)
        QCOMPARE(b.at(i), b.at(b.size() - 1 - i));

    // bassInner flips which end meets in the middle: lowest band at the seam,
    // highest at both outer edges. Still symmetric.
    bars->setProperty("bassInner", true);
    bars->takeFrame(frame);
    QCOMPARE(bars->barCount(), 8);
    const QList<qreal> inner = bars->bands();
    QVERIFY(sameAs(inner, {0.4, 0.3, 0.2, 0.1, 0.1, 0.2, 0.3, 0.4}));
    for (int i = 0; i < inner.size(); ++i)
        QCOMPARE(inner.at(i), inner.at(inner.size() - 1 - i));

    // An unconnected audio input draws nothing rather than borrowing whichever
    // separator happened to come first, which is what the stack used to do.
    bars->clearFrame();
    QCOMPARE(bars->barCount(), 0);
    QCOMPARE(bars->bass(), 0.0);
}

void TestProject::nodeIdsAreStableAcrossSaveAndLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("ids.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    // Delete something first so the surviving ids are NOT 1..n in array order.
    // Under the old model ids were renumbered from 1 on every load, which is the
    // entire reason the hidden providerId only ever worked by coincidence.
    const int doomed = saveGraph.addNode(QStringLiteral("text"), 0, 0);
    const int kept = saveGraph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(saveGraph.removeNode(doomed));
    QVERIFY(kept > doomed);

    QList<int> before;
    for (const Node *node : saveGraph.nodes())
        before.append(node->nodeId());

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));

    QList<int> after;
    for (const Node *node : loadGraph.nodes())
        after.append(node->nodeId());

    QCOMPARE(after, before);
    QVERIFY(after.contains(kept));
    QVERIFY(!after.contains(doomed));

    // And a node added after the load must not collide with anything loaded.
    const int fresh = loadGraph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(fresh > 0);
    QVERIFY(!before.contains(fresh));
}

void TestProject::graphPositionsRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("positions.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();
    saveGraph.autoLayout();

    QHash<int, QPointF> placed;
    for (const Node *node : saveGraph.nodes()) {
        placed.insert(node->nodeId(), QPointF(node->property("graphX").toDouble(),
                                              node->property("graphY").toDouble()));
    }
    // autoLayout must actually have moved something, or this proves nothing.
    QVERIFY(std::any_of(placed.constBegin(), placed.constEnd(),
                        [](const QPointF &p) { return p.x() != 0.0 || p.y() != 0.0; }));

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));

    for (const Node *node : loadGraph.nodes()) {
        QVERIFY(placed.contains(node->nodeId()));
        const QPointF want = placed.value(node->nodeId());
        QCOMPARE(node->property("graphX").toDouble(), want.x());
        QCOMPARE(node->property("graphY").toDouble(), want.y());
    }
}

void TestProject::connectionsRoundTripByPortName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("edges.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    // A second spectrum on the same separator: one output, two inputs. This is
    // the relationship a layer stack could not express at all.
    const int second = saveGraph.addNode(QStringLiteral("spectrum"), 0, 0);
    SpectrumNode *first = saveGraph.spectrumNodes().first();
    auto *separator = qobject_cast<AudioSourceNode *>(first->input(QStringLiteral("audio")));
    QVERIFY(separator);
    QVERIFY(saveGraph.link(separator->nodeId(), second, QStringLiteral("audio")));
    QVERIFY(saveGraph.link(first->nodeId(), second, QStringLiteral("visual")));
    QVERIFY(saveGraph.link(second, saveGraph.outputNode()->nodeId(), QStringLiteral("visual")));

    const QSet<QString> before = edgeSet(saveGraph);
    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    // The port is stored as a NAME, not an index, so reordering the port table
    // would not silently reinterpret a file.
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray written = root.value(QStringLiteral("connections")).toArray();
    QCOMPARE(written.size(), before.size());
    for (const QJsonValue &value : written) {
        const QString port = value.toObject().value(QStringLiteral("port")).toString();
        QVERIFY2(port == QLatin1String("visual") || port == QLatin1String("audio"),
                 qPrintable(port));
    }

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));
    QCOMPARE(edgeSet(loadGraph), before);

    // And the fan-out survived as a fan-out.
    QCOMPARE(loadGraph.spectrumNodes().size(), 2);
    const int sourceA = loadGraph.spectrumNodes().at(0)->sourceId(QStringLiteral("audio"));
    const int sourceB = loadGraph.spectrumNodes().at(1)->sourceId(QStringLiteral("audio"));
    QVERIFY(sourceA != 0);
    QCOMPARE(sourceA, sourceB);
}

void TestProject::imageNodesRoundTripTheirSourceAndCrop()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("image.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    const int id = saveGraph.addNode(QStringLiteral("image"), 0, 0);
    QVERIFY(id > 0);
    Node *image = saveGraph.nodeById(id);
    QVERIFY(image);

    // Paths are stored absolute, as the file dialog hands them over.
    const QString source = QStringLiteral("file:///home/someone/pictures/cover art.png");
    image->setProperty("source", source);
    image->setProperty("fillMode", QStringLiteral("fit"));
    // All four off their defaults, and none of them equal to another, so a
    // transposed pair cannot pass.
    image->setProperty("cropX", 0.11);
    image->setProperty("cropY", 0.22);
    image->setProperty("cropW", 0.33);
    image->setProperty("cropH", 0.44);

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));

    Node *reloaded = loadGraph.nodeById(id);
    QVERIFY(reloaded);
    QCOMPARE(reloaded->typeId(), QStringLiteral("image"));
    QCOMPARE(reloaded->property("source").toString(), source);
    QCOMPARE(reloaded->property("fillMode").toString(), QStringLiteral("fit"));
    QCOMPARE(reloaded->property("cropX").toDouble(), 0.11);
    QCOMPARE(reloaded->property("cropY").toDouble(), 0.22);
    QCOMPARE(reloaded->property("cropW").toDouble(), 0.33);
    QCOMPARE(reloaded->property("cropH").toDouble(), 0.44);
}

void TestProject::transformNodesRoundTripTheirPlacement()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("transform.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    const int id = saveGraph.addNode(QStringLiteral("transform"), 0, 0);
    QVERIFY(id > 0);
    Node *transform = saveGraph.nodeById(id);
    QVERIFY(transform);

    transform->setProperty("offsetX", -0.25);
    transform->setProperty("offsetY", 0.4);
    transform->setProperty("scaleX", 1.75);
    transform->setProperty("scaleY", 0.6);
    transform->setProperty("rotation", -33.0);
    transform->setProperty("pivotX", 0.2);
    transform->setProperty("pivotY", 0.8);

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));

    Node *reloaded = loadGraph.nodeById(id);
    QVERIFY(reloaded);
    QCOMPARE(reloaded->typeId(), QStringLiteral("transform"));

    // Walked rather than listed, so a property added to TransformNode later is
    // covered here without anyone remembering to come back.
    const QMetaObject *meta = transform->metaObject();
    for (int i = Node::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        if (!prop.isWritable() || !prop.isStored())
            continue;
        QVERIFY2(prop.read(reloaded) == prop.read(transform),
                 qPrintable(QStringLiteral("property '%1' differs: %2 vs %3")
                                .arg(QString::fromUtf8(prop.name()),
                                     prop.read(reloaded).toString(),
                                     prop.read(transform).toString())));
    }
}

void TestProject::blurNodesRoundTripEverySetting()
{
    // Nothing in Project.cpp knows what a blur is: the save walks the
    // QMetaObject, so twelve new properties persist for free. This pins that --
    // a property declared without MEMBER/NOTIFY, or not stored, would silently
    // reset to its default on every load.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("blur.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    const int id = saveGraph.addNode(QStringLiteral("blur"), 0, 0);
    QVERIFY(id > 0);
    Node *blur = saveGraph.nodeById(id);
    QVERIFY(blur);

    // Every value differs from its default, so a property that failed to
    // persist comes back visibly wrong rather than accidentally right.
    blur->setProperty("gaussianEnabled", true);
    blur->setProperty("gaussianAmount", 0.075);
    blur->setProperty("chromaBlurEnabled", true);
    blur->setProperty("chromaBlurAmount", 0.14);
    blur->setProperty("chromaSplitEnabled", true);
    blur->setProperty("chromaSplitAmount", 0.023);
    blur->setProperty("maskEnabled", true);
    blur->setProperty("maskX", 0.3);
    blur->setProperty("maskY", 0.7);
    blur->setProperty("maskRadius", 0.42);
    blur->setProperty("maskSoftness", 0.9);
    blur->setProperty("maskInvert", true);

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));

    Node *reloaded = loadGraph.nodeById(id);
    QVERIFY(reloaded);
    QCOMPARE(reloaded->typeId(), QStringLiteral("blur"));

    // Walked rather than listed, so a property added to BlurNode later is
    // covered here without anyone remembering to come back.
    const QMetaObject *meta = blur->metaObject();
    for (int i = Node::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        if (!prop.isWritable() || !prop.isStored())
            continue;
        QVERIFY2(prop.read(reloaded) == prop.read(blur),
                 qPrintable(QStringLiteral("property '%1' differs: %2 vs %3")
                                .arg(QString::fromUtf8(prop.name()),
                                     prop.read(reloaded).toString(),
                                     prop.read(blur).toString())));
    }
}

void TestProject::aMergesTwoEdgesSurviveAsDistinctPorts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("merge.mvproj"));

    NodeGraph saveGraph;
    Project saveProject;
    saveProject.setGraph(&saveGraph);
    saveProject.newProject();

    const int back = saveGraph.addNode(QStringLiteral("background"), 0, 0);
    const int front = saveGraph.addNode(QStringLiteral("text"), 0, 0);
    const int merge = saveGraph.addNode(QStringLiteral("merge"), 0, 0);
    QVERIFY(back > 0 && front > 0 && merge > 0);

    QVERIFY(saveGraph.link(back, merge, QStringLiteral("background")));
    QVERIFY(saveGraph.link(front, merge, QStringLiteral("foreground")));
    QVERIFY(saveGraph.link(merge, saveGraph.outputNode()->nodeId(),
                           QStringLiteral("visual")));

    QVERIFY(saveProject.save(QUrl::fromLocalFile(path)));

    // Both edges are on disk under their own port names. Two edges into one node
    // is a shape the format has never had to carry before, and collapsing them
    // (or writing an index) would silently lose the compositing order.
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QStringList intoMerge;
    for (const QJsonValue &value : root.value(QStringLiteral("connections")).toArray()) {
        const QJsonObject edge = value.toObject();
        if (edge.value(QStringLiteral("to")).toInt() == merge)
            intoMerge.append(edge.value(QStringLiteral("port")).toString());
    }
    intoMerge.sort();
    QCOMPARE(intoMerge, QStringList({QStringLiteral("background"),
                                     QStringLiteral("foreground")}));

    NodeGraph loadGraph;
    Project loadProject;
    loadProject.setGraph(&loadGraph);
    QVERIFY2(loadProject.load(QUrl::fromLocalFile(path)), qPrintable(loadProject.lastError()));
    QCOMPARE(edgeSet(loadGraph), edgeSet(saveGraph));

    // And the two arms came back the right way round, not merely both present.
    Node *reloadedMerge = loadGraph.nodeById(merge);
    QVERIFY(reloadedMerge);
    QCOMPARE(reloadedMerge->sourceId(QStringLiteral("background")), back);
    QCOMPARE(reloadedMerge->sourceId(QStringLiteral("foreground")), front);

    const QVector<VisualNode *> drawn = loadGraph.renderList()->nodes();
    QCOMPARE(drawn.size(), 2);
    QCOMPARE(drawn.at(0)->nodeId(), back);
    QCOMPARE(drawn.at(1)->nodeId(), front);
}

void TestProject::loadingAFileWithoutAnOutputSynthesizesOne()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("no-output.mvproj"));

    // Neither essential node present. A project file that is slightly wrong should
    // open and be fixable rather than refuse.
    QVERIFY(writeProject(path, 3,
                         {jsonNode(7, QStringLiteral("background"))},
                         {}));

    NodeGraph graph;
    Project project;
    project.setGraph(&graph);
    QVERIFY2(project.load(QUrl::fromLocalFile(path)), qPrintable(project.lastError()));

    QVERIFY(graph.outputNode());
    QVERIFY(graph.audioNode());
    QCOMPARE(graph.count(), 3);
    // The synthesized nodes must not steal the id the file already used.
    QVERIFY(graph.nodeById(7));
    QVERIFY(graph.outputNode()->nodeId() != 7);
    QVERIFY(graph.audioNode()->nodeId() != 7);
    QVERIFY(graph.outputNode()->nodeId() != graph.audioNode()->nodeId());
}

void TestProject::loadingAFileWithAnInvalidConnectionDropsOnlyThatEdge()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("bad-edge.mvproj"));

    const QJsonArray nodes{
        jsonNode(1, QStringLiteral("output")),
        jsonNode(2, QStringLiteral("audio")),
        jsonNode(3, QStringLiteral("background")),
    };
    const QJsonArray edges{
        jsonEdge(3, 1, QStringLiteral("visual")),  // fine
        jsonEdge(2, 1, QStringLiteral("visual")),  // audio output into a visual input
        jsonEdge(3, 1, QStringLiteral("nonsense")), // no such port
        jsonEdge(3, 99, QStringLiteral("visual")),  // no such node
    };
    QVERIFY(writeProject(path, 3, nodes, edges));

    NodeGraph graph;
    Project project;
    project.setGraph(&graph);
    QTest::ignoreMessage(QtWarningMsg, "mv: dropping invalid connection 2 -> 1.visual");
    QTest::ignoreMessage(QtWarningMsg, "mv: dropping invalid connection 3 -> 1.nonsense");
    QTest::ignoreMessage(QtWarningMsg, "mv: dropping invalid connection 3 -> 99.visual");
    QVERIFY2(project.load(QUrl::fromLocalFile(path)), qPrintable(project.lastError()));

    // The good edge survived; the project was not lost over the bad ones.
    QCOMPARE(edgeSet(graph), QSet<QString>{QStringLiteral("3>1.visual")});
    QCOMPARE(graph.renderList()->nodes().size(), 1);
}

void TestProject::everyExampleProjectLoads()
{
    // The cheapest possible guard on the migration script's output: if a converted
    // file does not open, or opens with nothing to draw, the conversion was wrong.
    const QDir dir(QStringLiteral(MV_EXAMPLES_DIR));
    const QStringList files = dir.entryList({QStringLiteral("*.mvproj")}, QDir::Files);
    QVERIFY2(!files.isEmpty(), qPrintable(dir.absolutePath()));

    for (const QString &name : files) {
        NodeGraph graph;
        Project project;
        project.setGraph(&graph);
        QVERIFY2(project.load(QUrl::fromLocalFile(dir.filePath(name))),
                 qPrintable(name + QStringLiteral(": ") + project.lastError()));
        QVERIFY2(!graph.renderList()->nodes().isEmpty(), qPrintable(name));
        QVERIFY2(graph.outputNode(), qPrintable(name));
        QVERIFY2(graph.audioNode(), qPrintable(name));
    }
}

QTEST_MAIN(TestProject)
#include "tst_project.moc"
