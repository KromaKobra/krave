#include "NodeGraph.h"

#include <QHash>
#include <QQmlEngine>
#include <algorithm>

namespace mv {
namespace {

// Graph-space layout grid. Mirrored in tools/migrate_v2_to_v3.py.
constexpr qreal kMargin = 40.0;
constexpr qreal kColumnStep = 176.0 + 48.0; // Theme.nodeWidth + gap
constexpr qreal kRowStep = 62.0 + 88.0;     // Theme.nodeHeight + gap

/// Longest path to `node` from any node with no connected inputs.
///
/// Longest rather than shortest so a node always sits below everything feeding
/// it: with the shortest path, a spectrum fed by both a background (depth 0) and
/// a separator (depth 1) would land level with the separator.
int depthOf(const Node *node, QHash<const Node *, int> &memo, QSet<const Node *> &onPath)
{
    if (!node)
        return 0;
    const auto cached = memo.constFind(node);
    if (cached != memo.constEnd())
        return *cached;
    if (onPath.contains(node))
        return 0; // only reachable via a corrupt file; see RenderList::flatten

    onPath.insert(node);
    int deepest = -1;
    for (int i = 0; i < node->inputCount(); ++i) {
        if (const Node *source = node->input(i))
            deepest = std::max(deepest, depthOf(source, memo, onPath));
    }
    onPath.remove(node);

    const int d = deepest + 1;
    memo.insert(node, d);
    return d;
}

} // namespace

NodeGraph::NodeGraph(QObject *parent)
    : QAbstractListModel(parent)
    , m_renderList(std::make_unique<RenderList>(this))
{
    connect(this, &NodeGraph::topologyChanged, m_renderList.get(), &RenderList::rebuild);
}

NodeGraph::~NodeGraph() = default;

// ------------------------------------------------------------------ the model

int NodeGraph::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_nodes.size();
}

QVariant NodeGraph::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size())
        return {};

    Node *node = m_nodes.at(index.row());
    switch (role) {
    case NodeRole:
        return QVariant::fromValue(node);
    case NameRole:
        return node->name();
    case TypeRole:
        return node->typeId();
    case VisibleRole:
        // Only visual nodes have it; everything else reads as shown, which is
        // what an eye toggle on a separator should look like if one ever appears.
        if (auto *v = qobject_cast<VisualNode *>(node))
            return v->layerVisible();
        return true;
    default:
        return {};
    }
}

QHash<int, QByteArray> NodeGraph::roleNames() const
{
    return {
        {NodeRole, "nodeObject"},
        {NameRole, "name"},
        {TypeRole, "typeId"},
        {VisibleRole, "layerVisible"},
    };
}

// ------------------------------------------------------------------- creation

Node *NodeGraph::createNode(const QString &typeId, int nodeId, QObject *parent)
{
    Node *node = nullptr;
    if (typeId == QLatin1String("output"))
        node = new OutputNode(parent);
    else if (typeId == QLatin1String("audio"))
        node = new AudioNode(parent);
    else if (typeId == QLatin1String("separator"))
        node = new SeparatorNode(parent);
    else if (typeId == QLatin1String("spectrum"))
        node = new SpectrumNode(parent);
    else if (typeId == QLatin1String("background"))
        node = new BackgroundNode(parent);
    else if (typeId == QLatin1String("text"))
        node = new TextNode(parent);
    else if (typeId == QLatin1String("image"))
        node = new ImageNode(parent);
    else if (typeId == QLatin1String("merge"))
        node = new MergeNode(parent);
    else if (typeId == QLatin1String("transform"))
        node = new TransformNode(parent);
    else if (typeId == QLatin1String("blur"))
        node = new BlurNode(parent);

    if (node && nodeId > 0)
        node->setNodeId(nodeId);
    return node;
}

void NodeGraph::adoptNode(Node *node)
{
    node->setParent(this);
    if (node->nodeId() <= 0)
        node->setNodeId(m_nextNodeId++);
    // Covers both paths: a new node takes the next id, a loaded node keeps the
    // one from the file and pushes the counter past it.
    m_nextNodeId = std::max(m_nextNodeId, node->nodeId() + 1);
    QQmlEngine::setObjectOwnership(node, QQmlEngine::CppOwnership);

    // Re-emit any property edit as a document-level change so the canvas repaints
    // and the title bar can show a dirty marker. NOT topologyChanged: this fires
    // at 60 Hz while a node is being dragged.
    connect(node, &Node::changed, this, [this, node] {
        const int row = m_nodes.indexOf(node);
        if (row >= 0) {
            const QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx, {NameRole, VisibleRole});
        }
        Q_EMIT graphChanged();
    });
}

int NodeGraph::addNode(const QString &typeId, qreal x, qreal y)
{
    const NodeTypeSpec *spec = nodeTypeSpec(typeId);
    if (!spec)
        return 0;
    if (spec->essential) {
        qWarning("mv: %s is an essential node type and cannot be added",
                 qUtf8Printable(typeId));
        return 0;
    }

    Node *node = createNode(typeId, 0, this);
    if (!node)
        return 0;

    node->setProperty("graphX", x);
    node->setProperty("graphY", y);

    const int row = m_nodes.size();
    beginInsertRows({}, row, row);
    adoptNode(node);
    m_nodes.append(node);
    endInsertRows();

    Q_EMIT countChanged();
    Q_EMIT topologyChanged();
    Q_EMIT graphChanged();
    return node->nodeId();
}

bool NodeGraph::canRemove(int nodeId) const
{
    const Node *node = nodeById(nodeId);
    return node && !node->spec().essential;
}

bool NodeGraph::removeNode(int nodeId)
{
    if (!canRemove(nodeId))
        return false;

    Node *victim = nodeById(nodeId);
    const int row = m_nodes.indexOf(victim);
    if (row < 0)
        return false;

    // Break every edge into the victim first, so nothing downstream is left
    // holding a pointer to a deleted node.
    for (Node *node : m_nodes) {
        for (int port = 0; port < node->inputCount(); ++port) {
            if (node->input(port) == victim)
                node->setInput(port, nullptr);
        }
    }

    beginRemoveRows({}, row, row);
    m_nodes.removeAt(row);
    endRemoveRows();

    // deleteLater, so a QML binding still evaluating this node survives the turn.
    // Disconnect first or its dying property writes would re-dirty the document.
    disconnect(victim, nullptr, this, nullptr);
    victim->deleteLater();

    if (m_selectedNodeId == nodeId)
        setSelectedNodeId(0);

    Q_EMIT countChanged();
    Q_EMIT topologyChanged();
    Q_EMIT graphChanged();
    return true;
}

// -------------------------------------------------------------------- linking

bool NodeGraph::reachesUpstream(const Node *from, const Node *target,
                                QSet<const Node *> &seen)
{
    if (!from || seen.contains(from))
        return false;
    seen.insert(from);

    for (int port = 0; port < from->inputCount(); ++port) {
        const Node *source = from->input(port);
        if (!source)
            continue;
        if (source == target || reachesUpstream(source, target, seen))
            return true;
    }
    return false;
}

bool NodeGraph::canLink(int fromId, int toId, const QString &toPort) const
{
    if (fromId == toId)
        return false;

    Node *from = nodeById(fromId);
    Node *to = nodeById(toId);
    if (!from || !to)
        return false;

    const NodeTypeSpec &fromSpec = from->spec();
    if (!fromSpec.hasOutput)
        return false;

    const int port = to->portIndex(toPort);
    if (port < 0)
        return false;

    // `accepts` is what makes "a mask input takes a visual output" need no special
    // case here, in the hit test, or in the highlighting.
    if (!to->spec().inputs.at(port).accepts.contains(fromSpec.outputKind))
        return false;

    // A cycle would hang the flatten and audioRoot() alike, and nothing in a layer
    // stack could be cyclic -- this is a genuinely new failure mode. All input
    // ports are walked, audio included: separator -> separator -> separator loops
    // just as readily as a visual chain does.
    QSet<const Node *> seen;
    return !reachesUpstream(from, to, seen);
}

bool NodeGraph::link(int fromId, int toId, const QString &toPort)
{
    if (!canLink(fromId, toId, toPort))
        return false;

    Node *from = nodeById(fromId);
    Node *to = nodeById(toId);
    const int port = to->portIndex(toPort);

    if (to->input(port) == from)
        return true; // already so; do not churn the render list

    // Replace silently: dropping onto an occupied input overwrites it, which is
    // what every node editor does and what the gesture reads as.
    to->setInput(port, from);

    Q_EMIT topologyChanged();
    Q_EMIT graphChanged();
    return true;
}

void NodeGraph::unlink(int toId, const QString &toPort)
{
    Node *to = nodeById(toId);
    if (!to)
        return;

    const int port = to->portIndex(toPort);
    if (port < 0 || !to->input(port))
        return;

    to->setInput(port, nullptr);
    Q_EMIT topologyChanged();
    Q_EMIT graphChanged();
}

// ------------------------------------------------------------------ querying

Node *NodeGraph::nodeById(int id) const
{
    if (id <= 0)
        return nullptr;
    for (Node *node : m_nodes) {
        if (node->nodeId() == id)
            return node;
    }
    return nullptr;
}

Node *NodeGraph::nodeAt(int row) const
{
    if (row < 0 || row >= m_nodes.size())
        return nullptr;
    return m_nodes.at(row);
}

void NodeGraph::setSelectedNodeId(int id)
{
    const int resolved = nodeById(id) ? id : 0;
    if (m_selectedNodeId == resolved)
        return;
    m_selectedNodeId = resolved;
    Q_EMIT selectionChanged();
}

OutputNode *NodeGraph::outputNode() const
{
    for (Node *node : m_nodes) {
        if (auto *o = qobject_cast<OutputNode *>(node))
            return o;
    }
    return nullptr;
}

AudioNode *NodeGraph::audioNode() const
{
    for (Node *node : m_nodes) {
        if (auto *a = qobject_cast<AudioNode *>(node))
            return a;
    }
    return nullptr;
}

QVector<AudioSourceNode *> NodeGraph::liveAudioSources() const
{
    QVector<AudioSourceNode *> result;
    for (Node *node : m_nodes) {
        auto *source = qobject_cast<AudioSourceNode *>(node);
        if (source && source->audioRoot())
            result.append(source);
    }
    return result;
}

QVector<SpectrumNode *> NodeGraph::spectrumNodes() const
{
    QVector<SpectrumNode *> result;
    for (Node *node : m_nodes) {
        if (auto *s = qobject_cast<SpectrumNode *>(node))
            result.append(s);
    }
    return result;
}

QVariantList NodeGraph::connections() const
{
    QVariantList result;
    for (Node *to : m_nodes) {
        const QVector<PortSpec> &ports = to->spec().inputs;
        for (int port = 0; port < ports.size(); ++port) {
            Node *from = to->input(port);
            if (!from)
                continue;

            QVariantMap edge;
            edge.insert(QStringLiteral("fromId"), from->nodeId());
            edge.insert(QStringLiteral("toId"), to->nodeId());
            edge.insert(QStringLiteral("port"), ports.at(port).name);
            edge.insert(QStringLiteral("kind"), portKindName(ports.at(port).kind));
            // Index and count so the editor can place the endpoint without a
            // second lookup; the node objects so its geometry bindings track a
            // drag through graphX/graphY.
            edge.insert(QStringLiteral("portIndex"), port);
            edge.insert(QStringLiteral("portCount"), int(ports.size()));
            edge.insert(QStringLiteral("fromNode"), QVariant::fromValue(from));
            edge.insert(QStringLiteral("toNode"), QVariant::fromValue(to));
            result.append(edge);
        }
    }
    return result;
}

QVariantList NodeGraph::portsFor(const QString &typeId) const
{
    QVariantList result;
    const NodeTypeSpec *spec = nodeTypeSpec(typeId);
    if (!spec)
        return result;

    for (int i = 0; i < spec->inputs.size(); ++i) {
        const PortSpec &port = spec->inputs.at(i);
        QStringList accepts;
        for (PortKind kind : port.accepts)
            accepts.append(portKindName(kind));

        QVariantMap entry;
        entry.insert(QStringLiteral("name"), port.name);
        entry.insert(QStringLiteral("kind"), portKindName(port.kind));
        entry.insert(QStringLiteral("accepts"), accepts);
        entry.insert(QStringLiteral("required"), port.required);
        entry.insert(QStringLiteral("stacked"), port.stacked);
        entry.insert(QStringLiteral("label"), port.label);
        entry.insert(QStringLiteral("index"), i);
        entry.insert(QStringLiteral("count"), int(spec->inputs.size()));
        result.append(entry);
    }
    return result;
}

QString NodeGraph::outputKindFor(const QString &typeId) const
{
    const NodeTypeSpec *spec = nodeTypeSpec(typeId);
    if (!spec || !spec->hasOutput)
        return {};
    return portKindName(spec->outputKind);
}

QVariantList NodeGraph::creatableTypes() const
{
    QVariantList result;
    for (const NodeTypeSpec &spec : allNodeTypes()) {
        if (!spec.essential)
            result.append(spec.typeId);
    }
    return result;
}

// -------------------------------------------------------------- whole graph

void NodeGraph::clearNodes()
{
    for (Node *node : m_nodes)
        disconnect(node, nullptr, this, nullptr);
    qDeleteAll(m_nodes);
    m_nodes.clear();
}

void NodeGraph::resetNodes(const QVector<Node *> &nodes, const QVector<NodeEdge> &edges)
{
    beginResetModel();
    clearNodes();
    m_nextNodeId = 1;

    bool haveOutput = false;
    bool haveAudio = false;
    for (Node *node : nodes) {
        if (qobject_cast<OutputNode *>(node)) {
            if (haveOutput) {
                qWarning("mv: more than one output node in this project; dropping the extra");
                delete node;
                continue;
            }
            haveOutput = true;
        } else if (qobject_cast<AudioNode *>(node)) {
            if (haveAudio) {
                qWarning("mv: more than one audio node in this project; dropping the extra");
                delete node;
                continue;
            }
            haveAudio = true;
        }
        adoptNode(node);
        m_nodes.append(node);
    }

    // Synthesize rather than refuse: a project missing its root should open and
    // be fixable, not be unopenable.
    if (!haveOutput) {
        qWarning("mv: project has no output node; synthesizing one");
        Node *output = createNode(QStringLiteral("output"));
        adoptNode(output);
        m_nodes.append(output);
    }
    if (!haveAudio) {
        qWarning("mv: project has no audio node; synthesizing one");
        Node *audio = createNode(QStringLiteral("audio"));
        adoptNode(audio);
        m_nodes.append(audio);
    }

    endResetModel();

    // Resolve the edge list through canLink, dropping any that fail. Dropping
    // beats refusing the whole file -- a corrupt edge should not lose a project.
    for (const NodeEdge &edge : edges) {
        if (!link(edge.fromId, edge.toId, edge.port)) {
            qWarning("mv: dropping invalid connection %d -> %d.%s", edge.fromId, edge.toId,
                     qUtf8Printable(edge.port));
        }
    }

    m_selectedNodeId = 0;
    // Rebuilt BEFORE the signals below, so anything reacting to topologyChanged or
    // selectionChanged already sees the right draw list. topologyChanged rebuilds
    // it again, which the equality check makes a no-op.
    m_renderList->rebuild();

    Q_EMIT countChanged();
    Q_EMIT selectionChanged();
    Q_EMIT topologyChanged();
    Q_EMIT graphChanged();
}

void NodeGraph::loadDefaults()
{
    QVector<Node *> defaults;
    const auto make = [&defaults](const QString &typeId, int id) {
        Node *node = createNode(typeId, id);
        defaults.append(node);
        return node;
    };

    // Audio -> Separator -> (audio) Spectrum, and
    // Background -> (visual) Spectrum -> (visual) Output.
    make(QStringLiteral("output"), 1);
    make(QStringLiteral("audio"), 2);
    make(QStringLiteral("separator"), 3);
    make(QStringLiteral("background"), 4);
    make(QStringLiteral("spectrum"), 5);

    resetNodes(defaults, {
                             {2, 3, QStringLiteral("audio")},
                             {3, 5, QStringLiteral("audio")},
                             {4, 5, QStringLiteral("visual")},
                             {5, 1, QStringLiteral("visual")},
                         });
    autoLayout();
}

void NodeGraph::autoLayout()
{
    QHash<const Node *, int> depth;
    for (Node *node : m_nodes) {
        QSet<const Node *> onPath;
        depthOf(node, depth, onPath);
    }

    // Rows by depth, columns by node id within a row -- stable, so pressing
    // Arrange twice does not shuffle anything.
    QVector<Node *> ordered = m_nodes;
    std::sort(ordered.begin(), ordered.end(),
              [](const Node *a, const Node *b) { return a->nodeId() < b->nodeId(); });

    QHash<int, int> columnInRow;
    for (Node *node : ordered) {
        const int row = depth.value(node, 0);
        const int column = columnInRow[row]++;
        node->setProperty("graphX", kMargin + column * kColumnStep);
        node->setProperty("graphY", kMargin + row * kRowStep);
    }
}

} // namespace mv
