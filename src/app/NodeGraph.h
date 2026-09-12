#pragma once

#include "Node.h"
#include "RenderList.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantList>
#include <QVector>
#include <memory>

namespace mv {

/// One connection, as it appears on disk and when loading: ids and a port NAME.
struct NodeEdge
{
    int fromId = 0;
    int toId = 0;
    QString port;
};

/// The node graph: the document.
///
/// Still a QAbstractListModel, because the editor's Repeater wants stable
/// delegates across add and remove. The list is the set of nodes; it carries no
/// ordering meaning at all -- layering is a consequence of topology now, and
/// RenderList is what derives it.
class NodeGraph : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedNodeId READ selectedNodeId WRITE setSelectedNodeId NOTIFY selectionChanged)
    Q_PROPERTY(mv::Node *selectedNode READ selectedNode NOTIFY selectionChanged)
    Q_PROPERTY(mv::RenderList *renderList READ renderList CONSTANT)
    /// Every edge, for the editor's edge Repeater. Re-emits on topologyChanged
    /// only, so edges rebuild on a rewire and never on a drag.
    Q_PROPERTY(QVariantList connections READ connections NOTIFY topologyChanged)

public:
    enum Roles
    {
        NodeRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        VisibleRole,
    };

    explicit NodeGraph(QObject *parent = nullptr);
    ~NodeGraph() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return int(m_nodes.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ---------------------------------------------------------------- editing

    /// Create a node of `typeId` at graph position (x, y) and return its nodeId,
    /// or 0. Refuses essential types: the graph has exactly one Output and one
    /// Audio, and they are synthesized rather than added.
    Q_INVOKABLE int addNode(const QString &typeId, qreal x, qreal y);
    Q_INVOKABLE bool removeNode(int nodeId);
    Q_INVOKABLE bool canRemove(int nodeId) const;

    /// Would this edge be legal? Checks kind compatibility, that the port exists,
    /// and that it would not close a cycle.
    Q_INVOKABLE bool canLink(int fromId, int toId, const QString &toPort) const;
    /// Connect, replacing whatever was on `toPort` already. False if illegal.
    ///
    /// NOT called `connect`: a member of that name shadows QObject::connect inside
    /// every other member function of this class, which is a remarkably annoying
    /// way to lose an afternoon.
    Q_INVOKABLE bool link(int fromId, int toId, const QString &toPort);
    Q_INVOKABLE void unlink(int toId, const QString &toPort);

    Q_INVOKABLE mv::Node *nodeById(int id) const;
    /// The node at model row `row`, or nullptr. For QML code that has to sweep
    /// every node -- GraphLayout's port hit test -- without reaching into data()
    /// with a hard-coded role number.
    Q_INVOKABLE mv::Node *nodeAt(int row) const;

    /// This type's input ports, as [{ name, kind, accepts, stacked, label, index,
    /// count }] for the editor.
    Q_INVOKABLE QVariantList portsFor(const QString &typeId) const;
    /// "visual" | "audio" | "mask", or "" when the type has no output.
    Q_INVOKABLE QString outputKindFor(const QString &typeId) const;
    /// The typeIds the add menu may offer: everything that is not essential.
    Q_INVOKABLE QVariantList creatableTypes() const;

    /// Lay the graph out in rows by depth, flowing top to bottom. Must stay in
    /// step with tools/migrate_v2_to_v3.py, or a converted file jumps the first
    /// time anyone presses Arrange.
    Q_INVOKABLE void autoLayout();

    QVariantList connections() const;

    // --------------------------------------------------------------- querying

    RenderList *renderList() const { return m_renderList.get(); }

    int selectedNodeId() const { return m_selectedNodeId; }
    void setSelectedNodeId(int id);
    Node *selectedNode() const { return nodeById(m_selectedNodeId); }

    const QVector<Node *> &nodes() const { return m_nodes; }

    OutputNode *outputNode() const;
    AudioNode *audioNode() const;

    /// Audio sources worth analysing this frame: every AudioSourceNode whose
    /// audioRoot() resolves. A separator plugged into nothing has no samples.
    QVector<AudioSourceNode *> liveAudioSources() const;
    QVector<SpectrumNode *> spectrumNodes() const;

    // ------------------------------------------------------------ whole graph

    /// Build a node of the given type without inserting it. Used by project load,
    /// which passes the id read from the file.
    static Node *createNode(const QString &typeId, int nodeId = 0, QObject *parent = nullptr);

    /// Replace the whole graph, taking ownership of `nodes` and resolving `edges`.
    ///
    /// Defensive on purpose: a missing Output or Audio is synthesized and a
    /// duplicate Output is dropped, because a project file that is slightly wrong
    /// should open and be fixable rather than refuse.
    void resetNodes(const QVector<Node *> &nodes, const QVector<NodeEdge> &edges);

    /// Set up a reasonable starting project so a new user sees something.
    void loadDefaults();

Q_SIGNALS:
    void countChanged();
    void selectionChanged();

    /// Nodes or edges added or removed. RenderList listens to THIS one.
    ///
    /// Kept separate from graphChanged because dragging a node writes graphX at
    /// 60 Hz, and a render list that re-flattened on that would reset the canvas
    /// model every frame of every drag. This one line of signal design decides
    /// whether the editor feels good or broken.
    void topologyChanged();

    /// Anything at all, including a property edit. The document is dirty; Project
    /// listens to this. Moving a node IS an edit, so a drag belongs here.
    void graphChanged();

private:
    void adoptNode(Node *node);
    void clearNodes();
    /// True if `target` can be reached by walking inputs upstream from `from`.
    static bool reachesUpstream(const Node *from, const Node *target,
                                QSet<const Node *> &seen);

    QVector<Node *> m_nodes;
    std::unique_ptr<RenderList> m_renderList;
    int m_selectedNodeId = 0;

    /// Never reset and never reused within a session. Reuse would let a deleted
    /// node's SpectrumAnalyzer in SpectrumWorker::m_analyzers be inherited by a
    /// new node, cross-wiring two smoothing envelopes into what looks like random
    /// flicker. A document load restarts it from the loaded maximum.
    int m_nextNodeId = 1;
};

} // namespace mv
