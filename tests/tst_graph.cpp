#include "NodePorts.h"

#include <QSet>
#include <QTest>

using namespace mv;

/// Topology and the port table.
///
/// A separate binary from tst_project on purpose: a broken flatten and a broken
/// round-trip are different failures and should be reported as such.
class TestGraph : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void portTableCoversEveryNodeType();
    void portTableInvariantsHold();
    void maskInputsAcceptVisualOutputs();
    void portKindNamesRoundTrip();
};

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
    }

    // The set the graph is built around. Spelt out rather than derived, so
    // adding or dropping a type is a deliberate edit to this list.
    const QSet<QString> expected = {
        QStringLiteral("output"),    QStringLiteral("audio"),
        QStringLiteral("separator"), QStringLiteral("background"),
        QStringLiteral("spectrum"),  QStringLiteral("text"),
    };
    QCOMPARE(seen, expected);

    // Exactly two types are essential, and neither may be created or deleted.
    QStringList essential;
    for (const NodeTypeSpec &spec : all) {
        if (spec.essential)
            essential.append(spec.typeId);
    }
    QCOMPARE(essential, QStringList({QStringLiteral("output"), QStringLiteral("audio")}));
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
            if (port.kind == PortKind::Audio)
                QVERIFY2(!port.stacked, where.constData());
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

QTEST_MAIN(TestGraph)
#include "tst_graph.moc"
