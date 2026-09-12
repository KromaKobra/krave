#pragma once

#include <QString>
#include <QVector>

namespace mv {

/// What kind of data a port carries. The colour a port and its edges are drawn
/// in is a pure function of this, so the kind IS the visible type.
enum class PortKind
{
    Visual, ///< a rendered image — purple
    Audio,  ///< PCM or band data — orange
    Mask,   ///< reserved: an image sampled as a matte rather than stacked — blue
};

/// One port on a node type.
struct PortSpec
{
    /// "visual" | "audio". STABLE: this string is what gets serialized as the
    /// `port` field of a connection, so the on-disk edge list survives the port
    /// table being reordered.
    QString name;
    PortKind kind = PortKind::Visual;

    /// Inputs only: the output kinds that may be plugged in here. A Mask input
    /// declares {Visual}, so "blue accepts purple" needs no special case in the
    /// validator, in the hit test, or in the highlighting.
    QVector<PortKind> accepts;

    /// Inputs only: does the subgraph reaching this port join the draw list?
    ///
    /// True for a visual input, FALSE for a mask -- a mask is sampled as a
    /// texture by the node that owns it, not composited underneath it. Getting
    /// this wrong is how a mask would silently get drawn twice.
    bool stacked = false;

    QString label;
};

/// Everything the graph machinery needs to know about a node type.
///
/// This table lives in C++ rather than in QML because flattening, cycle
/// detection, the deletion cascade and link validation all read it, and all of
/// them must run headless in the tests with no QML engine anywhere. NodeTypes.qml
/// keeps presentation only -- icon, title, blurb.
struct NodeTypeSpec
{
    QString typeId;
    QVector<PortSpec> inputs;
    bool hasOutput = false;
    PortKind outputKind = PortKind::Visual;
    /// Cannot be created from the add menu or deleted. The graph has exactly one
    /// of each essential type and synthesizes a missing one on load.
    bool essential = false;
    /// Has a delegate in VisualizerCanvas, and so may appear in the render list.
    bool visual = false;
};

/// The spec for `typeId`, or nullptr with a qWarning if the type is unknown.
const NodeTypeSpec *nodeTypeSpec(const QString &typeId);

/// Every node type, in the order the add menu should offer them.
const QVector<NodeTypeSpec> &allNodeTypes();

/// "visual" | "audio" | "mask". Used in JSON and handed to QML, where
/// Theme.portColor() switches on it.
QString portKindName(PortKind kind);
PortKind portKindFromName(const QString &name);

} // namespace mv
