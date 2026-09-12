// One port on a node card, and the grab handle for wiring it.
//
// Inputs are circles, the output is a rounded square -- shape says direction, so
// the colour is free to mean only one thing: the kind of data. An unconnected port
// is dimmed, which is what makes a missing wire visible without reading any labels.
import QtQuick

import krave
import "../controls"

Item {
    id: dot

    required property var node
    required property var canvas
    /// A port record from NodeGraph.portsFor(), or a synthetic one for the output.
    required property var port
    required property bool isOutput

    readonly property color kindColor: Theme.portColor(dot.port.kind)
    readonly property bool connected:
        dot.isOutput ? dot.canvas.hasConsumers(dot.node)
                     : (dot.node && dot.node.sourceId(dot.port.name) !== 0)

    /// Would a drop here land? Answered WITHOUT hit-testing: each dot asks whether
    /// it accepts what is currently being dragged, so the compatible ones swell and
    /// everything else dims with no coordinate maths involved at all.
    readonly property bool compatible:
        dot.canvas.linking && !dot.isOutput
        && dot.port.accepts.indexOf(dot.canvas.linkKind) >= 0
        && dot.node.nodeId !== dot.canvas.linkFromId

    /// Dim what a drop cannot reach, so the legal targets are the only lit thing.
    readonly property bool dimmed: dot.canvas.linking && !dot.compatible

    /// A required input with nothing plugged in: the node will draw nothing, and
    /// that should be visible from the graph rather than only from the inspector.
    readonly property bool starved: !dot.isOutput && !dot.connected && !dot.canvas.linking

    // The visible dot is small; the hit area is not. 20 px is about the smallest
    // target that does not require aiming.
    width: 20
    height: 20

    // Swelling a compatible target is what makes the drop feel aimed rather than
    // dropped, and it costs no hit testing to decide.
    scale: dot.compatible ? 1.45 : 1.0
    opacity: dot.dimmed ? 0.3 : 1.0

    Behavior on scale { NumberAnimation { duration: Theme.durFast } }
    Behavior on opacity { NumberAnimation { duration: Theme.durFast } }

    // Dashed warning ring on a starved input.
    Rectangle {
        anchors.centerIn: parent
        visible: dot.starved
        width: Theme.portRadius * 4
        height: width
        radius: width / 2
        color: "transparent"
        border.width: 1
        border.color: Theme.withAlpha(Theme.warning, 0.5)
    }

    Rectangle {
        id: mark

        anchors.centerIn: parent
        width: Theme.portRadius * 2
        height: width
        radius: dot.isOutput ? Theme.radiusSm * 0.5 : width / 2
        color: dot.connected || dot.compatible ? dot.kindColor : Theme.nodeFill
        border.width: dot.connected || dot.compatible ? 0 : 1.5
        border.color: dot.starved ? Theme.warning : Theme.withAlpha(dot.kindColor, 0.55)

        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    AppToolTip {
        text: dot.isOutput
              ? qsTr("%1 output — drag to connect").arg(NodeTypes.title(dot.node.typeId))
              : dot.connected ? qsTr("%1 input — drag off to move or disconnect")
                                  .arg(dot.port.label)
                              : qsTr("%1 input — not connected").arg(dot.port.label)
        visible: hover.hovered && !dot.canvas.linking
        x: dot.width + Theme.space1
        y: 0
        width: Math.min(implicitWidth, 200)
    }

    HoverHandler { id: hover }

    // ------------------------------------------------------------------ wiring
    //
    // TimelinePanel's drag idiom again: press / positionChanged guarded on
    // `pressed` / released / canceled. LeftButton only, so a middle press still
    // falls through to the pan area.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.CrossCursor

        onPressed: (mouse) => {
            const at = mapToItem(dot.canvas.content, mouse.x, mouse.y)
            if (dot.isOutput) {
                dot.canvas.beginLink(dot.node.nodeId, dot.port.kind, at)
                return
            }

            // Pressing an already-connected input GRABS the existing edge and
            // re-drags it from its original source. Releasing over empty space
            // leaves it disconnected, so one gesture gives both "move this
            // connection" and "delete this connection" -- which is what every CAD
            // and node tool trains people to expect.
            const source = dot.node.sourceId(dot.port.name)
            if (source === 0)
                return
            dot.canvas.unlink(dot.node.nodeId, dot.port.name)
            dot.canvas.beginLink(source, dot.port.kind, at)
        }

        onPositionChanged: (mouse) => {
            if (!pressed || !dot.canvas.linking)
                return
            dot.canvas.dragLink(mapToItem(dot.canvas.content, mouse.x, mouse.y))
        }

        onReleased: dot.canvas.finishLink()
        onCanceled: dot.canvas.cancelLink()
    }
}
