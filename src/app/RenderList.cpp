#include "RenderList.h"
#include "NodeGraph.h"

namespace mv {

RenderList::RenderList(NodeGraph *graph)
    : QAbstractListModel(graph)
    , m_graph(graph)
{
}

int RenderList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant RenderList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case LayerRole:
        return QVariant::fromValue(static_cast<Node *>(entry.node));
    case TransformsRole: {
        QVariantList result;
        result.reserve(entry.transforms.size());
        for (Node *t : entry.transforms)
            result.append(QVariant::fromValue(t));
        return result;
    }
    case EffectsRole: {
        QVariantList result;
        result.reserve(entry.effects.size());
        for (Node *e : entry.effects)
            result.append(QVariant::fromValue(e));
        return result;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> RenderList::roleNames() const
{
    // Deliberately not "layer": a delegate exposing that role as a required
    // property would collide with QQuickItem's FINAL `layer` property.
    return {
        {LayerRole, "layerObject"},
        {TransformsRole, "layerTransforms"},
        {EffectsRole, "layerEffects"},
    };
}

void RenderList::flatten(Node *n, QVector<Entry> &out, QSet<const Node *> &onPath,
                         QVector<Node *> &stack, QVector<Node *> &effects)
{
    if (!n)
        return;
    if (onPath.contains(n)) {
        qWarning("mv: cycle in the visual chain at node %d", n->nodeId());
        return;
    }
    if (out.size() >= kMaxDrawCalls)
        return;

    onPath.insert(n);

    // Pushed BEFORE descending, so everything upstream inherits it. A transform
    // nested inside another therefore reports both, outer one first.
    const bool isTransform = qobject_cast<TransformNode *>(n) != nullptr;
    if (isTransform)
        stack.append(n);

    // Same push-before-descending shape, on its own stack: a blur nested inside
    // another reports both, outer one first, which is the order the shader
    // passes nest in.
    const bool isBlur = qobject_cast<BlurNode *>(n) != nullptr;
    if (isBlur)
        effects.append(n);

    const QVector<PortSpec> &ports = n->spec().inputs;
    for (int port = 0; port < ports.size(); ++port) {
        if (ports.at(port).stacked)
            flatten(n->input(port), out, onPath, stack, effects);
    }

    if (isTransform)
        stack.removeLast();
    if (isBlur)
        effects.removeLast();

    if (auto *v = qobject_cast<VisualNode *>(n))
        out.append(Entry{v, stack, effects});

    onPath.remove(n);
}

QVector<VisualNode *> RenderList::nodes() const
{
    QVector<VisualNode *> result;
    result.reserve(m_entries.size());
    for (const Entry &entry : m_entries)
        result.append(entry.node);
    return result;
}

void RenderList::rebuild()
{
    QVector<Entry> next;
    if (m_graph) {
        if (OutputNode *output = m_graph->outputNode()) {
            QSet<const Node *> onPath;
            QVector<Node *> stack;
            QVector<Node *> effects;
            // Seeded from the Output's input rather than the Output itself: it is
            // not a VisualNode, so it would not be appended anyway, but starting
            // inside the chain makes that independent of the class hierarchy.
            flatten(output->input(0), next, onPath, stack, effects);
        }
    }

    // Compare before touching the model: adding an unrelated node off to the side
    // changes the topology but not the draw order, and must not churn the canvas.
    // Entry::operator== covers the transform lists too, so re-plugging a
    // transform does rebuild even though the same nodes are drawn.
    if (next == m_entries)
        return;

    // A chain reorder is not expressible as a row move, and topology changes are
    // user-paced, so a full reset is both correct and cheap enough.
    beginResetModel();
    m_entries = next;
    endResetModel();
}

} // namespace mv
