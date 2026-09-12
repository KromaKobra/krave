// One node: a rectangle with a typed port on each edge it uses.
//
// x and y are BOUND to node.graphX/graphY and must stay bound. See the drag
// handler below for what assigning them instead would cost.
import QtQuick

import krave
import "../controls"

Rectangle {
    id: card

    /// The Node this card shows. Never null in practice -- the Repeater's model
    /// only produces live nodes -- but every binding tolerates it anyway.
    required property var node
    /// The GraphCanvas, for selection and the in-flight link state.
    required property var canvas

    readonly property bool selected: card.node && card.node.nodeId === nodeGraph.selectedNodeId
    readonly property string outputKind:
        card.node ? nodeGraph.outputKindFor(card.node.typeId) : ""
    readonly property var inputPorts: card.node ? nodeGraph.portsFor(card.node.typeId) : []
    readonly property bool isOutputNode: card.node && card.node.typeId === "output"
    readonly property bool hidden:
        card.node && card.node.layerVisible !== undefined && !card.node.layerVisible

    // Bindings, not assignments. GraphLayout's header comment explains why every
    // endpoint in the editor is arithmetic over these two numbers.
    x: card.node ? card.node.graphX : 0
    y: card.node ? card.node.graphY : 0
    width: Theme.nodeWidth
    height: Theme.nodeHeight

    radius: Theme.radiusMd
    color: Theme.nodeFill
    // Selection is a RING, never a fill: `accent` is a near neighbour of
    // portVisual, and an accent-filled card would be hard to tell from a node
    // whose visual port is lit up. The Output gets a stronger resting border
    // because it is the root of the render.
    border.width: card.selected ? 2 : 1
    border.color: card.selected ? Theme.accent
                : cardHover.hovered ? Theme.dividerStrong
                : card.isOutputNode ? Theme.withAlpha(Theme.text, 0.22)
                : Theme.divider

    Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

    HoverHandler { id: cardHover }

    // ------------------------------------------------------------------ header
    Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.nodeHeaderHeight
        // Square off the bottom corners so the header reads as a band rather than
        // as a floating pill inside the card.
        radius: card.radius
        color: Theme.nodeHeaderFill

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: card.radius
            color: parent.color
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.divider
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            spacing: Theme.space2

            AppIcon {
                anchors.verticalCenter: parent.verticalCenter
                path: card.node ? NodeTypes.icon(card.node.typeId) : ""
                size: 13
                color: card.selected ? Theme.accent : Theme.neutral400
                opacity: card.hidden ? 0.4 : 1
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 13 - Theme.space2 * 2
                text: card.node ? NodeTypes.title(card.node.typeId).toUpperCase() : ""
                color: Theme.textGhost
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeKicker
                font.letterSpacing: 0.5
                elide: Text.ElideRight
            }
        }
    }

    // -------------------------------------------------------------------- name
    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        verticalAlignment: Text.AlignVCenter
        text: card.node ? card.node.name : ""
        color: card.hidden ? Theme.textFaint : Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        font.weight: card.selected ? Theme.weightMedium : Font.Normal
        elide: Text.ElideRight
    }

    AppIcon {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space2
        visible: card.hidden
        path: Icons.eyeOff
        size: 12
        color: Theme.textFaint
    }

    // ------------------------------------------------------------------- ports
    //
    // Inputs along the top edge, the single output centred on the bottom. Both sit
    // OUTSIDE the card's bounds by half a dot, so an edge meets the border rather
    // than disappearing under it -- hence clip staying false.
    Repeater {
        model: card.inputPorts

        delegate: PortDot {
            required property var modelData
            required property int index

            node: card.node
            canvas: card.canvas
            port: modelData
            isOutput: false
            x: Theme.nodeWidth * (index + 1) / (card.inputPorts.length + 1) - width / 2
            y: -height / 2
        }
    }

    PortDot {
        visible: card.outputKind !== ""
        node: card.node
        canvas: card.canvas
        port: ({ name: "out", kind: card.outputKind, accepts: [] })
        isOutput: true
        x: Theme.nodeWidth / 2 - width / 2
        y: Theme.nodeHeight - height / 2
    }

    // -------------------------------------------------------------- select/drag
    //
    // Left button only, so a MIDDLE press over a node is rejected here and falls
    // through to the pan MouseArea beneath the whole content layer. That is the
    // entire routing mechanism -- no hit-test special cases anywhere.
    MouseArea {
        id: dragArea

        anchors.fill: parent
        // Port dots straddle the top and bottom edges by half their 20 px hit area,
        // and this MouseArea is stacked above them. Inset by that half or the drag
        // handler eats the bottom of every port.
        anchors.topMargin: Theme.portRadius * 2
        anchors.bottomMargin: Theme.portRadius * 2
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.SizeAllCursor

        /// Cursor position in graph space at the moment of the press.
        property point grab: Qt.point(0, 0)
        property real startX: 0
        property real startY: 0

        onPressed: (mouse) => {
            nodeGraph.selectedNodeId = card.node.nodeId
            // mapToItem is fine HERE: a handler runs once, it is not a binding.
            dragArea.grab = mapToItem(card.canvas.content, mouse.x, mouse.y)
            dragArea.startX = card.node.graphX
            dragArea.startY = card.node.graphY
        }

        // Deliberately NOT drag.target. That assigns card.x/card.y directly, which
        // permanently destroys the `x: node.graphX` binding above -- the node would
        // move on screen, nothing would be written to the model, and it would snap
        // back the moment anything re-evaluated or the project reloaded.
        //
        // mapToItem(content, ...) is correct even though the card is moving under
        // the cursor: the card moved by d, so mouse.x dropped by d while card.x
        // rose by d, and the sum is still the true graph-space cursor position.
        onPositionChanged: (mouse) => {
            if (!pressed)
                return
            const p = mapToItem(card.canvas.content, mouse.x, mouse.y)
            card.node.graphX = dragArea.startX + (p.x - dragArea.grab.x)
            card.node.graphY = dragArea.startY + (p.y - dragArea.grab.y)
        }

        // Snap on release only. Snapping during the drag makes the node feel
        // notchy and makes the edges twitch with it.
        onReleased: card.canvas.snapToGrid(card.node)

        onDoubleClicked: card.canvas.renameRequested(card.node.nodeId)
    }
}
