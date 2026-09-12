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
/// Walking the Output's visual input backwards depth-first produces exactly the
/// ordered array VisualizerCanvas's Repeater consumes, and its `z: index` then
/// means "downstream nodes draw on top" -- which is the semantics the graph
/// wants. A Merge does not break that: its two subtrees simply land one after the
/// other, background arm first, because port order is compositing order.
///
/// A Transform does not break it either, and that is the less obvious half. A
/// transform encloses a whole subgraph, but because it is affine and pivoted in
/// canvas coordinates, transforming each member of a group individually is the
/// same picture as transforming the group -- so the list stays FLAT and each
/// entry just records which transforms enclose it. See TransformNode's header for
/// why this holds for transform and would not for a group opacity or blend.
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
    /// One draw call: a visual node and the transform nodes enclosing it,
    /// OUTERMOST FIRST -- which is the order their matrices must be composed in.
    struct Entry
    {
        VisualNode *node = nullptr;
        QVector<Node *> transforms;

        /// Both members, so re-plugging a transform rebuilds the list even
        /// though the set of nodes drawn has not changed. See rebuild().
        bool operator==(const Entry &other) const
        {
            return node == other.node && transforms == other.transforms;
        }
        bool operator!=(const Entry &other) const { return !(*this == other); }
    };

    enum Roles
    {
        LayerRole = Qt::UserRole + 1,
        TransformsRole,
    };

    /// Pathological-graph guard. A corrupt file with a deeply reused subgraph can
    /// expand combinatorially; 256 draw calls is already far past anything usable.
    static constexpr int kMaxDrawCalls = 256;

    explicit RenderList(NodeGraph *graph);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<Entry> &entries() const { return m_entries; }

    /// Just the nodes drawn, in draw order. A projection rather than the storage:
    /// most callers (and every ordering test) care about what is drawn and not
    /// about what encloses it.
    QVector<VisualNode *> nodes() const;

    /// Re-flatten from the graph's Output. Connected to NodeGraph::topologyChanged
    /// and NOTHING else -- see the comment on that signal.
    void rebuild();

    /// Walk `n`'s stacked inputs depth-first and append the visual nodes found,
    /// bottom of the stack first.
    ///
    /// `stack` is the transform nodes currently enclosing the recursion, pushed
    /// on the way down and popped on the way out; each appended entry takes a
    /// copy. The recursion descends FROM the Output, so the push order is
    /// outermost-first, which is the order the matrices compose in.
    ///
    /// Four properties worth knowing:
    ///
    ///   - Only `stacked` inputs are followed. This is the line that stops a
    ///     future mask from being drawn as a layer as well as sampled as a matte.
    ///   - Port order is compositing order: input 0 ends up furthest back. This
    ///     is what makes a Merge's `background` port land behind `foreground`.
    ///   - A shared upstream subgraph is drawn TWICE, on purpose. `onPath` is a
    ///     path set, not a global visited set, so it breaks cycles without
    ///     deduplicating legitimate reuse -- and the two paths can legitimately
    ///     carry different transforms.
    ///   - Only VisualNodes are appended, so Merge, Transform and the Output
    ///     itself organise the list without ever appearing in it.
    ///
    /// The cycle guard is needed IN ADDITION TO NodeGraph::canLink's, because a
    /// hand-edited or corrupt file never goes through canLink, and unguarded
    /// recursion on A->B->A is a hard crash rather than a bad picture.
    static void flatten(Node *n, QVector<Entry> &out, QSet<const Node *> &onPath,
                        QVector<Node *> &stack);

private:
    NodeGraph *m_graph = nullptr;
    QVector<Entry> m_entries;
};

} // namespace mv
