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
    return parent.isValid() ? 0 : m_nodes.size();
}

QVariant RenderList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size())
        return {};
    if (role != LayerRole)
        return {};
    return QVariant::fromValue(static_cast<Node *>(m_nodes.at(index.row())));
}

QHash<int, QByteArray> RenderList::roleNames() const
{
    // Deliberately not "layer": a delegate exposing that role as a required
    // property would collide with QQuickItem's FINAL `layer` property.
    return {{LayerRole, "layerObject"}};
}

void RenderList::flatten(Node *n, QVector<VisualNode *> &out, QSet<const Node *> &onPath)
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

    const QVector<PortSpec> &ports = n->spec().inputs;
    for (int port = 0; port < ports.size(); ++port) {
        if (ports.at(port).stacked)
            flatten(n->input(port), out, onPath);
    }

    if (auto *v = qobject_cast<VisualNode *>(n))
        out.append(v);

    onPath.remove(n);
}

void RenderList::rebuild()
{
    QVector<VisualNode *> next;
    if (m_graph) {
        if (OutputNode *output = m_graph->outputNode()) {
            QSet<const Node *> onPath;
            // Seeded from the Output's input rather than the Output itself: it is
            // not a VisualNode, so it would not be appended anyway, but starting
            // inside the chain makes that independent of the class hierarchy.
            flatten(output->input(0), next, onPath);
        }
    }

    // Compare before touching the model: adding an unrelated node off to the side
    // changes the topology but not the draw order, and must not churn the canvas.
    if (next == m_nodes)
        return;

    // A chain reorder is not expressible as a row move, and topology changes are
    // user-paced, so a full reset is both correct and cheap enough.
    beginResetModel();
    m_nodes = next;
    endResetModel();
}

} // namespace mv
