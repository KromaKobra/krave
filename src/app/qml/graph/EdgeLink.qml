// One connection: a vertical cubic from a node's output to a port on another's top.
//
// Every coordinate here is arithmetic over the two nodes' graphX/graphY via
// GraphLayout, so the curve follows a dragged node live. Nothing in this file calls
// mapToItem -- see GraphLayout's header for why that would silently stop updating.
import QtQuick
import QtQuick.Shapes

import krave

Shape {
    id: link

    /// A connection record from NodeGraph.connections().
    required property var edge

    readonly property point p1: GraphLayout.outputPos(link.edge.fromNode)
    readonly property point p2: GraphLayout.inputPos(link.edge.toNode, link.edge.portIndex,
                                                    link.edge.portCount)
    readonly property real bow: GraphLayout.curveBow(link.p1, link.p2)
    readonly property bool touchesSelection:
        nodeGraph.selectedNodeId !== 0
        && (link.edge.fromId === nodeGraph.selectedNodeId
            || link.edge.toId === nodeGraph.selectedNodeId)

    // As AppIcon does: the curve renderer keeps a 2 px stroke clean at any angle,
    // and these are mostly diagonal.
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        strokeColor: Theme.withAlpha(Theme.portColor(link.edge.kind),
                                    link.touchesSelection ? 1.0 : 0.7)
        strokeWidth: link.touchesSelection ? 2.5 : 2
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap

        startX: link.p1.x
        startY: link.p1.y

        // Control points are purely VERTICAL offsets because the graph flows top to
        // bottom: the curve leaves an output downward and enters an input downward,
        // so two edges arriving at neighbouring ports stay visually separate.
        PathCubic {
            x: link.p2.x
            y: link.p2.y
            control1X: link.p1.x
            control1Y: link.p1.y + link.bow
            control2X: link.p2.x
            control2Y: link.p2.y - link.bow
        }
    }
}
