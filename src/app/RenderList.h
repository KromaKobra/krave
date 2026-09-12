#pragma once

#include "Node.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSet>
#include <QVector>

namespace mv {

class NodeGraph;

/// The flattened draw order: the visual subgraph reaching the Output, bottom
/// first.
///
/// The insight that keeps the whole rendering path intact is that every visual
/// node has at most one visual input, so the subgraph reaching the Output is a
/// chain. Walking that input backwards produces exactly the ordered array
/// VisualizerCanvas's Repeater already consumed, and its `z: index` then means
/// "downstream nodes draw on top" -- which is the semantics the graph wants.
///
/// A QAbstractListModel rather than a QVariantList property: a list property
/// would make the Repeater destroy and recreate every delegate on any topology
/// change, and would force the delegate off the `layerObject` role onto
/// `modelData`. The role name is kept as "layerObject" so VisualizerCanvas.qml
/// needs no structural change at all -- and because those delegates genuinely do
/// render a layer of the composite.
class RenderList : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        LayerRole = Qt::UserRole + 1,
    };

    /// Pathological-graph guard. A corrupt file with a deeply reused subgraph can
    /// expand combinatorially; 256 draw calls is already far past anything usable.
    static constexpr int kMaxDrawCalls = 256;

    explicit RenderList(NodeGraph *graph);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<VisualNode *> &nodes() const { return m_nodes; }

    /// Re-flatten from the graph's Output. Connected to NodeGraph::topologyChanged
    /// and NOTHING else -- see the comment on that signal.
    void rebuild();

    /// Walk `n`'s stacked inputs depth-first and append the visual nodes found,
    /// bottom of the stack first.
    ///
    /// Three properties worth knowing:
    ///
    ///   - Only `stacked` inputs are followed. This is the line that stops a
    ///     future mask from being drawn as a layer as well as sampled as a matte.
    ///   - Port order is compositing order: input 0 ends up furthest back.
    ///   - A shared upstream subgraph is drawn TWICE, on purpose. `onPath` is a
    ///     path set, not a global visited set, so it breaks cycles without
    ///     deduplicating legitimate reuse.
    ///
    /// The cycle guard is needed IN ADDITION TO NodeGraph::canLink's, because a
    /// hand-edited or corrupt file never goes through canLink, and unguarded
    /// recursion on A->B->A is a hard crash rather than a bad picture.
    static void flatten(Node *n, QVector<VisualNode *> &out, QSet<const Node *> &onPath);

private:
    NodeGraph *m_graph = nullptr;
    QVector<VisualNode *> m_nodes;
};

} // namespace mv
