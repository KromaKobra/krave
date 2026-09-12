#include "NodeGraph.h"
#include "NodePorts.h"

#include <QSet>
#include <QSignalSpy>
#include <QTest>

using namespace mv;

namespace {

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

} // namespace

/// Topology: the port table, link validation, the flatten.
///
/// A separate binary from tst_project on purpose: a broken flatten and a broken
/// round-trip are different failures and should be reported as such.
class TestGraph : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- the port table
    void portTableCoversEveryNodeType();
    void portTableInvariantsHold();
    void maskInputsAcceptVisualOutputs();
    void onlyDataDependenciesAreRequired();
    void portKindNamesRoundTrip();

    // ---- the graph
    void defaultProjectHasExactlyOneOutputAndOneAudio();
    void essentialNodesCannotBeCreatedOrRemoved();
    void mismatchedTypesCannotConnect();
    void oneOutputFeedsManyInputs();
    void connectingAnOccupiedInputReplacesTheEdge();
    void cyclesAreRejected();
    void flattenSurvivesACycleBuiltByHand();
    void deletingANodeBreaksItsConnections();
    void renderListIgnoresPropertyChanges();
    void renderListSkipsUnreachableNodes();
    void renderListDrawsASharedSubgraphTwice();
    void mergeDrawsItsBackgroundArmFirst();
    void aCycleThroughAMergesSecondInputIsRefused();
    void transformsAccumulateOutermostFirst();
    void blursAccumulateOutermostFirst();
    void organizingNodesNeverAppearInTheRenderList();
    void nodeIdsAreNotReusedAfterDeletion();
    void unconnectedSeparatorsAreNotAnalyzed();
    void chainedSeparatorsBothResolveToTheSameAudioRoot();
};

// ------------------------------------------------------------- the port table

void TestGraph::portTableCoversEveryNodeType()
{
    const QVector<NodeTypeSpec> &all = allNodeTypes();
    QVERIFY(!all.isEmpty());

    // Every type the table lists is findable by id, and no id appears twice --
    // nodeTypeSpec() returns the first match, so a duplicate would shadow.
    QSet<QString> seen;
    for (const NodeTypeSpec &spec : all) {
        QVERIFY2(!seen.contains(spec.typeId), qPrintable(spec.typeId));
        seen.insert(spec.typeId);

        const NodeTypeSpec *found = nodeTypeSpec(spec.typeId);
        QVERIFY2(found, qPrintable(spec.typeId));
        QCOMPARE(found->typeId, spec.typeId);

        // And the factory knows how to build it. Without this, a type could sit in
        // the add menu and silently fail to appear when clicked.
        Node *node = NodeGraph::createNode(spec.typeId);
        QVERIFY2(node, qPrintable(spec.typeId));
        QCOMPARE(node->typeId(), spec.typeId);
        QCOMPARE(node->inputCount(), spec.inputs.size());
        delete node;
    }

    // The set the graph is built around. Spelt out rather than derived, so
    // adding or dropping a type is a deliberate edit to this list.
    const QSet<QString> expected = {
        QStringLiteral("output"),    QStringLiteral("audio"),
        QStringLiteral("separator"), QStringLiteral("background"),
        QStringLiteral("spectrum"),  QStringLiteral("text"),
        QStringLiteral("image"),     QStringLiteral("merge"),
        QStringLiteral("transform"), QStringLiteral("blur"),
    };
    QCOMPARE(seen, expected);

    // Exactly two types are essential, and neither may be created or deleted.
    QStringList essential;
    for (const NodeTypeSpec &spec : all) {
        if (spec.essential)
            essential.append(spec.typeId);
    }
    QCOMPARE(essential, QStringList({QStringLiteral("output"), QStringLiteral("audio")}));

    // createNode refuses a type it has never heard of rather than substituting one.
    QTest::ignoreMessage(QtWarningMsg, "mv: no node type spec for 'particles'");
    QVERIFY(!nodeTypeSpec(QStringLiteral("particles")));
    QVERIFY(!NodeGraph::createNode(QStringLiteral("particles")));
}

void TestGraph::portTableInvariantsHold()
{
    for (const NodeTypeSpec &spec : allNodeTypes()) {
        QSet<QString> names;
        for (const PortSpec &port : spec.inputs) {
            const QByteArray where = (spec.typeId + QLatin1Char('.') + port.name).toUtf8();

            // Port names are serialized, so they must be non-empty and unique
            // within a node -- a connection is keyed by (toId, portName).
            QVERIFY2(!port.name.isEmpty(), qPrintable(spec.typeId));
            QVERIFY2(!names.contains(port.name), where.constData());
            names.insert(port.name);

            // An input that accepts nothing can never be connected.
            QVERIFY2(!port.accepts.isEmpty(), where.constData());

            // An audio edge selects a source; it never contributes pixels.
            if (port.kind == PortKind::Audio) {
                QVERIFY2(!port.stacked, where.constData());
                // And a node with no audio has nothing to work with at all, so
                // every audio input is a real dependency rather than an option.
                QVERIFY2(port.required, where.constData());
            }
        }

        // Output is the root of the render and so has nothing downstream of it;
        // everything else produces something or it could not be wired in.
        if (spec.typeId == QLatin1String("output"))
            QVERIFY(!spec.hasOutput);
        else
            QVERIFY2(spec.hasOutput, qPrintable(spec.typeId));

        // A node that draws must be able to sit in a visual chain.
        if (spec.visual)
            QCOMPARE(spec.outputKind, PortKind::Visual);
    }
}

void TestGraph::maskInputsAcceptVisualOutputs()
{
    // Pins the rule "blue accepts purple" before any node declares a mask port:
    // a mask is a Visual image sampled as a matte, so it must accept Visual
    // outputs and must NOT join the draw list -- a stacked mask would be
    // composited underneath its owner as well as sampled by it.
    //
    // Vacuous today by design. It starts biting the moment a mask port is added,
    // which is the point at which getting it wrong would be expensive.
    int maskPorts = 0;
    for (const NodeTypeSpec &spec : allNodeTypes()) {
        for (const PortSpec &port : spec.inputs) {
            if (port.kind != PortKind::Mask)
                continue;
            ++maskPorts;
            QVERIFY(port.accepts.contains(PortKind::Visual));
            QVERIFY(!port.stacked);
        }
    }
    qInfo("mask input ports in the table: %d", maskPorts);
}

void TestGraph::onlyDataDependenciesAreRequired()
{
    // The editor draws a warning ring on an unconnected REQUIRED input, so getting
    // this wrong either hides a real problem or covers every ordinary graph in
    // false alarms and teaches people to ignore the marker.
    //
    // The rule: an input is required when the node produces nothing without it. A
    // spectrum with nothing on its visual input is just the bottom of the chain.
    const auto requiredPorts = [](const QString &typeId) {
        QStringList names;
        const NodeTypeSpec *spec = nodeTypeSpec(typeId);
        for (const PortSpec &port : spec->inputs) {
            if (port.required)
                names.append(port.name);
        }
        return names;
    };

    // Nothing reaches the Output's input and nothing renders at all.
    QCOMPARE(requiredPorts(QStringLiteral("output")), QStringList{QStringLiteral("visual")});
    // No audio, no bands.
    QCOMPARE(requiredPorts(QStringLiteral("separator")), QStringList{QStringLiteral("audio")});
    // A spectrum needs its audio but NOT something behind it.
    QCOMPARE(requiredPorts(QStringLiteral("spectrum")), QStringList{QStringLiteral("audio")});
    // Text over nothing is a perfectly ordinary title card.
    QCOMPARE(requiredPorts(QStringLiteral("text")), QStringList());
    // A half-wired merge is a pass-through, not a mistake -- flagging it would
    // mean a warning ring on every merge the moment it is dropped on the canvas.
    QCOMPARE(requiredPorts(QStringLiteral("merge")), QStringList());
    // And a transform over nothing simply transforms nothing.
    QCOMPARE(requiredPorts(QStringLiteral("transform")), QStringList());
    // The image has no inputs at all; stacking is Merge's job.
    QCOMPARE(requiredPorts(QStringLiteral("image")), QStringList());
}

void TestGraph::portKindNamesRoundTrip()
{
    // These strings are the on-disk `kind` and what Theme.portColor() switches
    // on, so the mapping has to be total and stable in both directions.
    for (PortKind kind : {PortKind::Visual, PortKind::Audio, PortKind::Mask}) {
        const QString name = portKindName(kind);
        QVERIFY(!name.isEmpty());
        QCOMPARE(portKindFromName(name), kind);
    }

    QCOMPARE(portKindName(PortKind::Visual), QStringLiteral("visual"));
    QCOMPARE(portKindName(PortKind::Audio), QStringLiteral("audio"));
    QCOMPARE(portKindName(PortKind::Mask), QStringLiteral("mask"));

    // An unknown name falls back to Visual rather than asserting: a hand-edited
    // file should lose one edge at most, not fail to load.
    QCOMPARE(portKindFromName(QStringLiteral("nonsense")), PortKind::Visual);
}

// -------------------------------------------------------------------- the graph

void TestGraph::defaultProjectHasExactlyOneOutputAndOneAudio()
{
    NodeGraph graph;
    graph.loadDefaults();

    int outputs = 0;
    int audios = 0;
    for (Node *node : graph.nodes()) {
        if (node->typeId() == QLatin1String("output"))
            ++outputs;
        if (node->typeId() == QLatin1String("audio"))
            ++audios;
    }
    QCOMPARE(outputs, 1);
    QCOMPARE(audios, 1);

    // And it is a working project out of the box: a user should see bars move
    // without having to discover that an analyser is needed first.
    QCOMPARE(graph.renderList()->nodes().size(), 2);
    QCOMPARE(graph.liveAudioSources().size(), 2);
    SpectrumNode *spectrum = graph.spectrumNodes().first();
    QVERIFY(spectrum->sourceId(QStringLiteral("audio")) != 0);
    QVERIFY(spectrum->sourceId(QStringLiteral("visual")) != 0);
}

void TestGraph::essentialNodesCannotBeCreatedOrRemoved()
{
    NodeGraph graph;
    graph.loadDefaults();
    const int before = graph.count();

    QTest::ignoreMessage(QtWarningMsg,
                         "mv: output is an essential node type and cannot be added");
    QCOMPARE(graph.addNode(QStringLiteral("output"), 0, 0), 0);
    QTest::ignoreMessage(QtWarningMsg,
                         "mv: audio is an essential node type and cannot be added");
    QCOMPARE(graph.addNode(QStringLiteral("audio"), 0, 0), 0);
    QCOMPARE(graph.count(), before);

    const int outputId = graph.outputNode()->nodeId();
    const int audioId = graph.audioNode()->nodeId();
    QVERIFY(!graph.canRemove(outputId));
    QVERIFY(!graph.canRemove(audioId));
    QVERIFY(!graph.removeNode(outputId));
    QVERIFY(!graph.removeNode(audioId));
    QCOMPARE(graph.count(), before);

    // Nor do they appear in what the add menu is allowed to offer.
    const QVariantList creatable = graph.creatableTypes();
    QVERIFY(!creatable.contains(QVariant(QStringLiteral("output"))));
    QVERIFY(!creatable.contains(QVariant(QStringLiteral("audio"))));
    QVERIFY(creatable.contains(QVariant(QStringLiteral("spectrum"))));

    // A non-essential node still goes.
    const int text = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(graph.canRemove(text));
    QVERIFY(graph.removeNode(text));
    QCOMPARE(graph.count(), before);

    // And a node that does not exist is not removable either.
    QVERIFY(!graph.canRemove(0));
    QVERIFY(!graph.canRemove(9999));
}

void TestGraph::mismatchedTypesCannotConnect()
{
    NodeGraph graph;
    graph.loadDefaults();

    const int audio = graph.audioNode()->nodeId();
    const int spectrum = graph.spectrumNodes().first()->nodeId();
    const int background = graph.addNode(QStringLiteral("background"), 0, 0);

    // An audio output into a visual input, and a visual output into an audio
    // input. canLink and link must agree: the QML side runs canLink for the hover
    // preview, and if the two ever disagreed a drop would look legal and fail.
    for (const auto &bad : QVector<QPair<QPair<int, int>, QString>>{
             {{audio, spectrum}, QStringLiteral("visual")},
             {{background, spectrum}, QStringLiteral("audio")},
         }) {
        QVERIFY(!graph.canLink(bad.first.first, bad.first.second, bad.second));
        QVERIFY(!graph.link(bad.first.first, bad.first.second, bad.second));
        QCOMPARE(graph.nodeById(bad.first.second)->sourceId(bad.second) == bad.first.first,
                 false);
    }

    // A port that does not exist on the target.
    QVERIFY(!graph.canLink(background, spectrum, QStringLiteral("mask")));
    // The Output has no output of its own, so nothing can be driven from it.
    QVERIFY(!graph.canLink(graph.outputNode()->nodeId(), spectrum, QStringLiteral("visual")));
    // And nothing connects to itself.
    QVERIFY(!graph.canLink(background, background, QStringLiteral("visual")));

    // The matching cases still work, so the test is not just rejecting everything.
    QVERIFY(graph.canLink(background, spectrum, QStringLiteral("visual")));
}

void TestGraph::oneOutputFeedsManyInputs()
{
    NodeGraph graph;
    graph.loadDefaults();

    auto *separator = qobject_cast<AudioSourceNode *>(
        graph.spectrumNodes().first()->input(QStringLiteral("audio")));
    QVERIFY(separator);

    // Three spectrum nodes on one separator: one FFT, three things drawing it.
    // The relationship a flat stack had no way to express.
    QVector<int> spectra{graph.spectrumNodes().first()->nodeId()};
    for (int i = 0; i < 2; ++i) {
        const int id = graph.addNode(QStringLiteral("spectrum"), 0, 0);
        QVERIFY(graph.link(separator->nodeId(), id, QStringLiteral("audio")));
        spectra.append(id);
    }

    for (int id : spectra)
        QCOMPARE(graph.nodeById(id)->sourceId(QStringLiteral("audio")), separator->nodeId());

    // The cost is still one analysis per source, not one per consumer.
    QCOMPARE(graph.liveAudioSources().size(), 2);
}

void TestGraph::connectingAnOccupiedInputReplacesTheEdge()
{
    NodeGraph graph;
    graph.loadDefaults();

    const int spectrum = graph.spectrumNodes().first()->nodeId();
    const int firstSource = graph.nodeById(spectrum)->sourceId(QStringLiteral("visual"));
    QVERIFY(firstSource != 0);

    const int text = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(graph.link(text, spectrum, QStringLiteral("visual")));

    // Replaced, not added: an input is one slot, so there is nowhere for a second
    // source to go even if something tried.
    QCOMPARE(graph.nodeById(spectrum)->sourceId(QStringLiteral("visual")), text);

    int incoming = 0;
    for (int port = 0; port < graph.nodeById(spectrum)->inputCount(); ++port) {
        if (graph.nodeById(spectrum)->input(port))
            ++incoming;
    }
    QCOMPARE(incoming, 2); // one visual, one audio

    graph.unlink(spectrum, QStringLiteral("visual"));
    QCOMPARE(graph.nodeById(spectrum)->sourceId(QStringLiteral("visual")), 0);
}

void TestGraph::cyclesAreRejected()
{
    NodeGraph graph;
    graph.loadDefaults();

    // Two-node visual loop.
    const int a = graph.addNode(QStringLiteral("text"), 0, 0);
    const int b = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(graph.link(a, b, QStringLiteral("visual")));
    QVERIFY(!graph.canLink(b, a, QStringLiteral("visual")));
    QVERIFY(!graph.link(b, a, QStringLiteral("visual")));

    // Three-node visual loop: the walk has to go more than one hop.
    const int c = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(graph.link(b, c, QStringLiteral("visual")));
    QVERIFY(!graph.canLink(c, a, QStringLiteral("visual")));

    // An AUDIO loop, which is the one that is easy to forget: a
    // separator -> separator -> separator cycle hangs audioRoot() exactly as
    // readily as a visual cycle hangs the flatten, so canLink walks every input
    // port rather than only the stacked ones.
    const int s1 = graph.addNode(QStringLiteral("separator"), 0, 0);
    const int s2 = graph.addNode(QStringLiteral("separator"), 0, 0);
    const int s3 = graph.addNode(QStringLiteral("separator"), 0, 0);
    QVERIFY(graph.link(s1, s2, QStringLiteral("audio")));
    QVERIFY(graph.link(s2, s3, QStringLiteral("audio")));
    QVERIFY(!graph.canLink(s3, s1, QStringLiteral("audio")));
    QVERIFY(!graph.canLink(s2, s1, QStringLiteral("audio")));

    // A cycle that would close through a MIXED chain -- spectrum's visual input
    // downstream, its audio input upstream -- is caught too.
    const int spectrum = graph.spectrumNodes().first()->nodeId();
    QVERIFY(graph.link(s3, spectrum, QStringLiteral("audio")));
    QVERIFY(!graph.canLink(spectrum, s1, QStringLiteral("audio")));
}

void TestGraph::flattenSurvivesACycleBuiltByHand()
{
    // canLink is not allowed to be the only guard. It currently makes a cycle
    // unreachable through every public path -- including a corrupt file, because
    // resetNodes applies the edge list through canLink one edge at a time -- but
    // unguarded recursion on A->B->A is a hard crash rather than a bad picture, and
    // "unreachable today" is a weak thing to stake a crash on.
    //
    // m_inputs is private to NodeGraph, so a real loop cannot be built from here.
    // Instead flatten is entered with its path set already containing the node,
    // which is EXACTLY the state the recursion reaches on the second visit to a
    // node in a cycle -- the same branch, reached the same way.
    NodeGraph graph;
    minimalGraph(graph);
    const int a = graph.addNode(QStringLiteral("text"), 0, 0);
    const int b = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(graph.link(a, b, QStringLiteral("visual")));
    QVERIFY(graph.link(b, graph.outputNode()->nodeId(), QStringLiteral("visual")));
    QVERIFY(!graph.link(b, a, QStringLiteral("visual"))); // refused, as it must be

    Node *nodeA = graph.nodeById(a);
    Node *nodeB = graph.nodeById(b);

    // The sane case first, so the guard below is not passing for the wrong reason.
    QVector<RenderList::Entry> out;
    QSet<const Node *> onPath;
    QVector<Node *> stack;
    QVector<Node *> effects;
    RenderList::flatten(nodeB, out, onPath, stack, effects);
    QCOMPARE(out.size(), 2);
    QVERIFY(onPath.isEmpty()); // the walk leaves the path set as it found it
    QVERIFY(stack.isEmpty()); // and leaves the transform stack as it found it too
    QVERIFY(effects.isEmpty()); // and the effect stack likewise

    // Re-entered on the path: terminate, warn, contribute nothing.
    QVector<RenderList::Entry> again;
    QSet<const Node *> reentered{nodeA};
    QVector<Node *> againStack;
    QVector<Node *> againEffects;
    QTest::ignoreMessage(QtWarningMsg,
                         qPrintable(QStringLiteral("mv: cycle in the visual chain at node %1")
                                        .arg(a)));
    RenderList::flatten(nodeA, again, reentered, againStack, againEffects);
    QVERIFY(again.isEmpty());

    // rebuild() over the real graph terminates and produces the chain.
    graph.renderList()->rebuild();
    QCOMPARE(graph.renderList()->nodes().size(), 2);
}

void TestGraph::deletingANodeBreaksItsConnections()
{
    NodeGraph graph;
    graph.loadDefaults();

    // background -> victim -> spectrum -> output, then delete the middle.
    const int spectrum = graph.spectrumNodes().first()->nodeId();
    const int background = graph.nodeById(spectrum)->sourceId(QStringLiteral("visual"));
    const int victim = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(graph.link(background, victim, QStringLiteral("visual")));
    QVERIFY(graph.link(victim, spectrum, QStringLiteral("visual")));
    QCOMPARE(graph.renderList()->nodes().size(), 3);

    QVERIFY(graph.removeNode(victim));

    // The downstream input is null, not dangling.
    QCOMPARE(graph.nodeById(spectrum)->sourceId(QStringLiteral("visual")), 0);
    QVERIFY(!graph.nodeById(victim));

    // And the render list shrank rather than holding a pointer to a dead node.
    const QVector<VisualNode *> drawn = graph.renderList()->nodes();
    QCOMPARE(drawn.size(), 1);
    QCOMPARE(drawn.first()->nodeId(), spectrum);

    // Deleting the selection clears it rather than leaving it pointing at nothing.
    const int another = graph.addNode(QStringLiteral("text"), 0, 0);
    graph.setSelectedNodeId(another);
    QCOMPARE(graph.selectedNodeId(), another);
    QVERIFY(graph.removeNode(another));
    QCOMPARE(graph.selectedNodeId(), 0);
    QVERIFY(!graph.selectedNode());
}

void TestGraph::renderListIgnoresPropertyChanges()
{
    // This is what protects the drag path. Dragging a node writes graphX at 60 Hz;
    // if that reached the render list it would beginResetModel the canvas model
    // every frame of every drag, and the preview would strobe.
    NodeGraph graph;
    graph.loadDefaults();

    SpectrumNode *spectrum = graph.spectrumNodes().first();
    QSignalSpy topology(&graph, &NodeGraph::topologyChanged);
    QSignalSpy document(&graph, &NodeGraph::graphChanged);

    spectrum->setProperty("colorA", QColor(QStringLiteral("#123456")));
    spectrum->setProperty("graphX", 512.0);
    spectrum->setProperty("graphY", 256.0);
    spectrum->setName(QStringLiteral("Renamed"));

    QCOMPARE(topology.count(), 0);
    // But they ARE edits: moving a node dirties the document.
    QVERIFY(document.count() >= 4);

    // Adding an unrelated node off to the side changes the topology but not the
    // draw order, so the canvas model must not churn either.
    const QVector<VisualNode *> before = graph.renderList()->nodes();
    QSignalSpy reset(graph.renderList(), &QAbstractItemModel::modelReset);
    const int stray = graph.addNode(QStringLiteral("separator"), 0, 0);
    QVERIFY(stray > 0);
    QCOMPARE(topology.count(), 1);
    QCOMPARE(graph.renderList()->nodes(), before);
    QCOMPARE(reset.count(), 0);
}

void TestGraph::renderListSkipsUnreachableNodes()
{
    NodeGraph graph;
    graph.loadDefaults();
    const int reachable = graph.renderList()->nodes().size();
    QVERIFY(reachable > 0);

    // A visual node wired to nothing downstream is not part of the render.
    const int orphan = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(orphan > 0);
    QCOMPARE(graph.renderList()->nodes().size(), reachable);

    // Unplugging the Output empties the list entirely.
    graph.unlink(graph.outputNode()->nodeId(), QStringLiteral("visual"));
    QVERIFY(graph.renderList()->nodes().isEmpty());

    // Plugging the orphan in puts exactly it there.
    QVERIFY(graph.link(orphan, graph.outputNode()->nodeId(), QStringLiteral("visual")));
    QCOMPARE(graph.renderList()->nodes().size(), 1);
    QCOMPARE(graph.renderList()->nodes().first()->nodeId(), orphan);
}

void TestGraph::renderListDrawsASharedSubgraphTwice()
{
    // `onPath` is a PATH set, not a global visited set, so it breaks cycles
    // without deduplicating legitimate reuse -- a subgraph reached by two stacked
    // inputs is meant to be drawn once per path.
    NodeGraph graph;
    minimalGraph(graph);

    const int background = graph.addNode(QStringLiteral("background"), 0, 0);
    const int merge = graph.addNode(QStringLiteral("merge"), 0, 0);
    QVERIFY(background > 0 && merge > 0);

    // One node into BOTH of the merge's inputs: a diamond, not a cycle.
    QVERIFY(graph.link(background, merge, QStringLiteral("background")));
    QVERIFY(graph.link(background, merge, QStringLiteral("foreground")));
    QVERIFY(graph.link(merge, graph.outputNode()->nodeId(), QStringLiteral("visual")));

    const QVector<VisualNode *> drawn = graph.renderList()->nodes();
    QCOMPARE(drawn.size(), 2);
    QCOMPARE(drawn.at(0)->nodeId(), background);
    QCOMPARE(drawn.at(1)->nodeId(), background);
}

void TestGraph::mergeDrawsItsBackgroundArmFirst()
{
    // Port index IS compositing order, and `background` is index 0. This is the
    // whole of Merge's behaviour -- there is no code on the class at all.
    NodeGraph graph;
    minimalGraph(graph);

    const int back = graph.addNode(QStringLiteral("background"), 0, 0);
    const int front = graph.addNode(QStringLiteral("text"), 0, 0);
    const int merge = graph.addNode(QStringLiteral("merge"), 0, 0);
    QVERIFY(back > 0 && front > 0 && merge > 0);

    QVERIFY(graph.link(back, merge, QStringLiteral("background")));
    QVERIFY(graph.link(front, merge, QStringLiteral("foreground")));
    QVERIFY(graph.link(merge, graph.outputNode()->nodeId(), QStringLiteral("visual")));

    QVector<VisualNode *> drawn = graph.renderList()->nodes();
    QCOMPARE(drawn.size(), 2);
    QCOMPARE(drawn.at(0)->nodeId(), back);
    QCOMPARE(drawn.at(1)->nodeId(), front);

    // Swap the two edges and the order flips -- which is what makes the two
    // ports meaningfully different rather than just differently named.
    QVERIFY(graph.link(front, merge, QStringLiteral("background")));
    QVERIFY(graph.link(back, merge, QStringLiteral("foreground")));

    drawn = graph.renderList()->nodes();
    QCOMPARE(drawn.size(), 2);
    QCOMPARE(drawn.at(0)->nodeId(), front);
    QCOMPARE(drawn.at(1)->nodeId(), back);
}

void TestGraph::aCycleThroughAMergesSecondInputIsRefused()
{
    // Merge is the first type with two stacked visual inputs, so it is also the
    // first that could close a visual loop through a port the old chain never
    // had. canLink walks every input port, so this needs no special case -- but
    // "needs no special case" is exactly the kind of claim worth pinning.
    NodeGraph graph;
    minimalGraph(graph);

    const int merge = graph.addNode(QStringLiteral("merge"), 0, 0);
    const int text = graph.addNode(QStringLiteral("text"), 0, 0);
    QVERIFY(merge > 0 && text > 0);

    QVERIFY(graph.link(merge, text, QStringLiteral("visual")));

    // text is downstream of merge, so feeding it back into EITHER input closes
    // a loop.
    QVERIFY(!graph.canLink(text, merge, QStringLiteral("background")));
    QVERIFY(!graph.canLink(text, merge, QStringLiteral("foreground")));
    QVERIFY(!graph.link(text, merge, QStringLiteral("foreground")));

    // And a merge cannot feed itself.
    QVERIFY(!graph.canLink(merge, merge, QStringLiteral("background")));
}

void TestGraph::transformsAccumulateOutermostFirst()
{
    // A transform applies to its whole upstream subgraph, and the list stays
    // flat: each drawn node just records which transforms enclose it. Order
    // matters -- matrices do not commute -- and the recursion descends from the
    // Output, so the outermost transform must be first.
    NodeGraph graph;
    minimalGraph(graph);

    const int inner = graph.addNode(QStringLiteral("transform"), 0, 0);
    const int outer = graph.addNode(QStringLiteral("transform"), 0, 0);
    const int under = graph.addNode(QStringLiteral("background"), 0, 0);
    const int sibling = graph.addNode(QStringLiteral("text"), 0, 0);
    const int merge = graph.addNode(QStringLiteral("merge"), 0, 0);
    QVERIFY(inner > 0 && outer > 0 && under > 0 && sibling > 0 && merge > 0);

    // under -> inner -> outer -> merge.background, and sibling straight into
    // merge.foreground, so the two arms differ only in what encloses them.
    QVERIFY(graph.link(under, inner, QStringLiteral("visual")));
    QVERIFY(graph.link(inner, outer, QStringLiteral("visual")));
    QVERIFY(graph.link(outer, merge, QStringLiteral("background")));
    QVERIFY(graph.link(sibling, merge, QStringLiteral("foreground")));
    QVERIFY(graph.link(merge, graph.outputNode()->nodeId(), QStringLiteral("visual")));

    const QVector<RenderList::Entry> entries = graph.renderList()->entries();
    QCOMPARE(entries.size(), 2);

    QCOMPARE(entries.at(0).node->nodeId(), under);
    QCOMPARE(entries.at(0).transforms.size(), 2);
    QCOMPARE(entries.at(0).transforms.at(0)->nodeId(), outer);
    QCOMPARE(entries.at(0).transforms.at(1)->nodeId(), inner);

    // The sibling arm is untouched: a transform is not a global setting.
    QCOMPARE(entries.at(1).node->nodeId(), sibling);
    QVERIFY(entries.at(1).transforms.isEmpty());
}

void TestGraph::blursAccumulateOutermostFirst()
{
    // The same enclosing-stack mechanism transforms use, on a second stack --
    // and the reason Entry::effects exists at all. A blur draws nothing, so if
    // this stack were not recorded the node would have no effect whatsoever.
    NodeGraph graph;
    minimalGraph(graph);

    const int inner = graph.addNode(QStringLiteral("blur"), 0, 0);
    const int outer = graph.addNode(QStringLiteral("blur"), 0, 0);
    const int spectrum = graph.addNode(QStringLiteral("spectrum"), 0, 0);
    QVERIFY(inner > 0 && outer > 0 && spectrum > 0);

    // spectrum -> inner -> outer -> output.
    QVERIFY(graph.link(spectrum, inner, QStringLiteral("visual")));
    QVERIFY(graph.link(inner, outer, QStringLiteral("visual")));
    QVERIFY(graph.link(outer, graph.outputNode()->nodeId(), QStringLiteral("visual")));

    QVector<RenderList::Entry> entries = graph.renderList()->entries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).node->nodeId(), spectrum);
    // Nested blurs report both, and the ORDER is what the QML chain recurses
    // down: outermost node is the outermost pass.
    QCOMPARE(entries.at(0).effects.size(), 2);
    QCOMPARE(entries.at(0).effects.at(0)->nodeId(), outer);
    QCOMPARE(entries.at(0).effects.at(1)->nodeId(), inner);
    // And the blurs themselves are organizers, not layers.
    QCOMPARE(graph.renderList()->nodes().size(), 1);

    // Re-plugging a blur changes no node in the draw list, so only
    // Entry::operator== comparing `effects` makes the model rebuild. Without
    // that, dropping a blur out of the chain would leave the canvas blurred.
    QSignalSpy reset(graph.renderList(), &QAbstractItemModel::modelReset);
    QVERIFY(graph.link(spectrum, outer, QStringLiteral("visual")));
    QCOMPARE(reset.count(), 1);

    entries = graph.renderList()->entries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).node->nodeId(), spectrum);
    QCOMPARE(entries.at(0).effects.size(), 1);
    QCOMPARE(entries.at(0).effects.first()->nodeId(), outer);
}

void TestGraph::organizingNodesNeverAppearInTheRenderList()
{
    // Merge, Transform, Blur and the Output shape the draw list without being
    // drawn. If any of them were a VisualNode it would land in the list and the
    // canvas would try to find a delegate for it.
    NodeGraph graph;
    minimalGraph(graph);

    const int merge = graph.addNode(QStringLiteral("merge"), 0, 0);
    const int transform = graph.addNode(QStringLiteral("transform"), 0, 0);
    const int blur = graph.addNode(QStringLiteral("blur"), 0, 0);
    const int image = graph.addNode(QStringLiteral("image"), 0, 0);
    QVERIFY(merge > 0 && transform > 0 && blur > 0 && image > 0);

    QVERIFY(graph.link(image, merge, QStringLiteral("background")));
    QVERIFY(graph.link(merge, blur, QStringLiteral("visual")));
    QVERIFY(graph.link(blur, transform, QStringLiteral("visual")));
    QVERIFY(graph.link(transform, graph.outputNode()->nodeId(), QStringLiteral("visual")));

    const QVector<VisualNode *> drawn = graph.renderList()->nodes();
    QCOMPARE(drawn.size(), 1);
    QCOMPARE(drawn.first()->nodeId(), image);

    // Stated as the class-level fact it rests on, not just as an absence from
    // this one list.
    QVERIFY(!qobject_cast<VisualNode *>(graph.nodeById(merge)));
    QVERIFY(!qobject_cast<VisualNode *>(graph.nodeById(transform)));
    QVERIFY(!qobject_cast<VisualNode *>(graph.nodeById(blur)));
    QVERIFY(qobject_cast<VisualNode *>(graph.nodeById(image)));

    // The image is enclosed by both even with a merge in between: the stacks
    // follow the recursion, not the immediate parent.
    const RenderList::Entry &entry = graph.renderList()->entries().first();
    QCOMPARE(entry.transforms.size(), 1);
    QCOMPARE(entry.transforms.first()->nodeId(), transform);
    QCOMPARE(entry.effects.size(), 1);
    QCOMPARE(entry.effects.first()->nodeId(), blur);
}

void TestGraph::nodeIdsAreNotReusedAfterDeletion()
{
    // Reuse would let a deleted node's SpectrumAnalyzer in SpectrumWorker be
    // inherited by a new node, cross-wiring two smoothing envelopes into what
    // looks like random flicker.
    NodeGraph graph;
    graph.loadDefaults();

    QSet<int> everSeen;
    for (Node *node : graph.nodes())
        everSeen.insert(node->nodeId());

    for (int i = 0; i < 20; ++i) {
        const int id = graph.addNode(QStringLiteral("separator"), 0, 0);
        QVERIFY(id > 0);
        QVERIFY2(!everSeen.contains(id), qPrintable(QString::number(id)));
        everSeen.insert(id);
        QVERIFY(graph.removeNode(id));
    }
}

void TestGraph::unconnectedSeparatorsAreNotAnalyzed()
{
    NodeGraph graph;
    graph.loadDefaults();
    QCOMPARE(graph.liveAudioSources().size(), 2);

    // A separator plugged into nothing has no PCM to analyse, so it costs nothing.
    const int stray = graph.addNode(QStringLiteral("separator"), 0, 0);
    auto *strayNode = qobject_cast<AudioSourceNode *>(graph.nodeById(stray));
    QVERIFY(strayNode);
    QVERIFY(!strayNode->audioRoot());
    QCOMPARE(graph.liveAudioSources().size(), 2);

    // Wire it to the audio node and it becomes live.
    QVERIFY(graph.link(graph.audioNode()->nodeId(), stray, QStringLiteral("audio")));
    QCOMPARE(strayNode->audioRoot(), graph.audioNode());
    QCOMPARE(graph.liveAudioSources().size(), 3);

    // The audio node is its own root, always.
    QCOMPARE(graph.audioNode()->audioRoot(), graph.audioNode());
}

void TestGraph::chainedSeparatorsBothResolveToTheSameAudioRoot()
{
    // Chained separators do NOT cascade: each analyses the same PCM independently,
    // with its own band count and smoothing. Allowed, because forbidding it would
    // be a special case in an otherwise uniform type system -- but it is not a
    // two-stage filter, and this test is where that is written down.
    NodeGraph graph;
    graph.loadDefaults();

    auto *first = qobject_cast<AudioSourceNode *>(
        graph.spectrumNodes().first()->input(QStringLiteral("audio")));
    QVERIFY(first);

    const int secondId = graph.addNode(QStringLiteral("separator"), 0, 0);
    QVERIFY(graph.link(first->nodeId(), secondId, QStringLiteral("audio")));
    auto *second = qobject_cast<AudioSourceNode *>(graph.nodeById(secondId));
    QVERIFY(second);

    QCOMPARE(first->audioRoot(), graph.audioNode());
    QCOMPARE(second->audioRoot(), graph.audioNode());

    // Both are scheduled, and they really are independent: changing one's band
    // count does not change the other's.
    QCOMPARE(graph.liveAudioSources().size(), 3);
    second->setProperty("bandCount", 7);
    QCOMPARE(second->analysisParams().bandCount, 7);
    QCOMPARE(first->analysisParams().bandCount, 48);
}

QTEST_MAIN(TestGraph)
#include "tst_graph.moc"
