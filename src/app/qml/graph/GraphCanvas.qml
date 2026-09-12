// The pannable graph surface: grid, edges, nodes.
//
// Coordinates
// -----------
//   viewport  clips, and owns the pan MouseArea
//     content  GRAPH SPACE. Zero-sized on purpose -- it is a coordinate frame, not
//              a box, so nothing has to know how large the graph is.
//
// `content.x/y` are assigned imperatively by the pan handler and never bound.
// Everything inside content is positioned by arithmetic over graphX/graphY. No
// zoom yet; adding one means `content.scale` with transformOrigin TopLeft and a
// division by scale in the two mapping helpers below, and nothing else.
import QtQuick
import QtQuick.Shapes

import krave
import "../controls"

Item {
    id: viewport

    /// Emitted when a node card is double-clicked.
    signal renameRequested(int nodeId)

    readonly property alias content: content

    /// Grid step, and the snap the drag rounds to on release.
    readonly property int gridStep: 8

    // ------------------------------------------------------- in-flight link
    //
    // Held here rather than on the PortDot that started it, because every other
    // dot has to read it to decide whether it is a legal target -- which is what
    // lets the highlighting work with no hit testing at all.
    property bool linking: false
    property int linkFromId: 0
    property string linkKind: ""
    property point linkStart: Qt.point(0, 0)
    property point linkEnd: Qt.point(0, 0)
    /// The port the rubber band is currently snapped to, or null.
    property var linkTarget: null

    clip: true

    // ----------------------------------------------------------------- helpers

    /// Viewport coordinates -> graph space.
    function toGraph(x, y) {
        return Qt.point(x - content.x, y - content.y)
    }

    /// Graph space -> viewport coordinates.
    function fromGraph(x, y) {
        return Qt.point(x + content.x, y + content.y)
    }

    /// The centre of the visible area, in graph space. Where a new node goes.
    function viewportCentreInGraph() {
        return toGraph(width / 2 - Theme.nodeWidth / 2, height / 2 - Theme.nodeHeight / 2)
    }

    function snapToGrid(node) {
        if (!node)
            return
        node.graphX = Math.round(node.graphX / gridStep) * gridStep
        node.graphY = Math.round(node.graphY / gridStep) * gridStep
    }

    /// Does anything read this node's output? Used to dim unconnected output dots.
    function hasConsumers(node) {
        if (!node)
            return false
        const edges = nodeGraph.connections
        for (let i = 0; i < edges.length; ++i) {
            if (edges[i].fromId === node.nodeId)
                return true
        }
        return false
    }

    // --------------------------------------------------------------- wiring

    function beginLink(fromId, kind, at) {
        viewport.linkFromId = fromId
        viewport.linkKind = kind
        viewport.linkStart = GraphLayout.outputPos(nodeGraph.nodeById(fromId))
        viewport.linkEnd = at
        viewport.linkTarget = null
        viewport.linking = true
    }

    function dragLink(at) {
        const hit = GraphLayout.hitTestInput(nodeGraph, at.x, at.y, viewport.linkKind,
                                             viewport.linkFromId)
        viewport.linkTarget = hit
        // Snap the band to the target's centre and go solid, so the drop is
        // confirmed before the button comes up rather than after.
        viewport.linkEnd = hit ? hit.pos : at
    }

    function finishLink() {
        const hit = viewport.linkTarget
        // canLink ran in QML for the preview and runs again here in C++, which is
        // the authoritative check -- it is what catches cycles, and the QML side
        // has no business reasoning about those.
        if (hit && nodeGraph.canLink(viewport.linkFromId, hit.nodeId, hit.port))
            nodeGraph.link(viewport.linkFromId, hit.nodeId, hit.port)
        viewport.cancelLink()
    }

    function cancelLink() {
        viewport.linking = false
        viewport.linkTarget = null
        viewport.linkFromId = 0
        viewport.linkKind = ""
    }

    function unlink(nodeId, port) {
        nodeGraph.unlink(nodeId, port)
    }

    /// Bring the graph back into view: put its top-left corner near the top-left of
    /// the viewport. The way out of having panned somewhere empty.
    function resetView() {
        let minX = 0
        let minY = 0
        if (nodeGraph.count > 0) {
            minX = Infinity
            minY = Infinity
            for (let row = 0; row < nodeGraph.count; ++row) {
                const node = nodeGraph.nodeAt(row)
                minX = Math.min(minX, node.graphX)
                minY = Math.min(minY, node.graphY)
            }
        }
        content.x = Theme.space6 - minX
        content.y = Theme.space6 - minY
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.graphBg
    }

    // -------------------------------------------------------------------- grid
    //
    // Drawn in viewport space with the pan folded into the phase, so it scrolls with
    // the graph without needing to cover an unbounded content item.
    Canvas {
        id: grid

        anchors.fill: parent
        // Repaint on pan. The modulo keeps the value in one cell's range, so this
        // changes at most gridStep*4 distinct values however far the user pans.
        property real phaseX: ((content.x % (viewport.gridStep * 4)) + viewport.gridStep * 4)
                              % (viewport.gridStep * 4)
        property real phaseY: ((content.y % (viewport.gridStep * 4)) + viewport.gridStep * 4)
                              % (viewport.gridStep * 4)

        onPhaseXChanged: requestPaint()
        onPhaseYChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Theme.graphGrid
            const step = viewport.gridStep * 4
            for (let y = grid.phaseY - step; y < height + step; y += step) {
                for (let x = grid.phaseX - step; x < width + step; x += step)
                    ctx.fillRect(Math.round(x), Math.round(y), 1, 1)
            }
        }
    }

    // --------------------------------------------------------------------- pan
    //
    // FIRST child after the background, so it sits below `content` in z and only
    // sees presses the cards did not take. NodeCard's own MouseArea accepts
    // Qt.LeftButton only, so a MIDDLE press anywhere -- including over a card --
    // lands here and pans. That stacking IS the routing; there is no hit testing.
    //
    // Follows TimelinePanel's drag idiom (onPressed / onPositionChanged with a
    // `pressed` guard / onReleased / onCanceled) because it is the only one this
    // codebase has; no WheelHandler or DragHandler exists anywhere in the tree.
    MouseArea {
        id: panArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        cursorShape: panning ? Qt.ClosedHandCursor : Qt.ArrowCursor

        property bool panning: false
        property point origin: Qt.point(0, 0)
        property point startContent: Qt.point(0, 0)

        onPressed: (mouse) => {
            if (mouse.button === Qt.MiddleButton) {
                panArea.panning = true
                panArea.origin = Qt.point(mouse.x, mouse.y)
                panArea.startContent = Qt.point(content.x, content.y)
            } else {
                // A left press on empty space deselects. Anything over a card never
                // reaches here.
                nodeGraph.selectedNodeId = 0
            }
        }
        onPositionChanged: (mouse) => {
            if (!panArea.panning || !pressed)
                return
            content.x = panArea.startContent.x + (mouse.x - panArea.origin.x)
            content.y = panArea.startContent.y + (mouse.y - panArea.origin.y)
        }
        onReleased: panArea.panning = false
        onCanceled: panArea.panning = false
    }

    // ------------------------------------------------------------- graph space
    Item {
        id: content

        x: 0
        y: 0
        width: 1
        height: 1

        // Edges below the cards, so a wire passes behind a node rather than across
        // its name.
        Repeater {
            // A QVariantList that re-emits on topologyChanged ONLY, so the edge
            // delegates are rebuilt on a rewire and never on a drag. The curves
            // still follow a drag, because their geometry binds to graphX/graphY.
            model: nodeGraph.connections

            delegate: EdgeLink {
                required property var modelData
                edge: modelData
            }
        }

        // The rubber band sits above the edges and below the cards, so it passes
        // behind the node it is about to land on rather than across its name.
        Shape {
            id: rubberBand

            visible: viewport.linking
            preferredRendererType: Shape.CurveRenderer

            readonly property real bow:
                GraphLayout.curveBow(viewport.linkStart, viewport.linkEnd)

            ShapePath {
                strokeColor: Theme.portColor(viewport.linkKind)
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                // Dashed while it is going nowhere, solid the moment it snaps to a
                // port: free confirmation that the drop will land.
                strokeStyle: viewport.linkTarget ? ShapePath.SolidLine : ShapePath.DashLine
                dashPattern: [4, 4]

                startX: viewport.linkStart.x
                startY: viewport.linkStart.y

                PathCubic {
                    x: viewport.linkEnd.x
                    y: viewport.linkEnd.y
                    control1X: viewport.linkStart.x
                    control1Y: viewport.linkStart.y + rubberBand.bow
                    control2X: viewport.linkEnd.x
                    control2Y: viewport.linkEnd.y - rubberBand.bow
                }
            }
        }

        Repeater {
            model: nodeGraph

            delegate: NodeCard {
                required property var nodeObject

                node: nodeObject
                canvas: viewport
            }
        }
    }

    // -------------------------------------------------------------- empty copy
    Text {
        anchors.centerIn: parent
        width: parent.width - Theme.space8 * 2
        visible: nodeGraph.count === 0
        text: qsTr("The graph is empty.\nEvery project needs an Output node to render "
                   + "anything.")
        color: Theme.textGhost
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    // A new document has its own positions, and the old pan would very likely put
    // them off screen. Reset rather than leave the user looking at nothing.
    Connections {
        target: project
        function onLoaded() { viewport.resetView() }
    }

    Component.onCompleted: viewport.resetView()
}
