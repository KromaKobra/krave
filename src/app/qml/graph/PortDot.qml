// One port on a node card.
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

    // The visible dot is small; the hit area is not. 20 px is about the smallest
    // target that does not require aiming.
    width: 20
    height: 20

    Rectangle {
        id: mark

        anchors.centerIn: parent
        width: Theme.portRadius * 2
        height: width
        radius: dot.isOutput ? Theme.radiusSm * 0.5 : width / 2
        color: dot.connected ? dot.kindColor : Theme.nodeFill
        border.width: dot.connected ? 0 : 1.5
        border.color: dot.connected ? "transparent" : Theme.withAlpha(dot.kindColor, 0.55)

        Behavior on color { ColorAnimation { duration: Theme.durFast } }
        Behavior on scale { NumberAnimation { duration: Theme.durFast } }
    }

    AppToolTip {
        text: dot.isOutput ? qsTr("%1 output").arg(NodeTypes.title(dot.node.typeId))
                           : qsTr("%1 input").arg(dot.port.label)
        visible: hover.hovered
        x: dot.width + Theme.space1
        y: 0
    }

    HoverHandler { id: hover }
}
