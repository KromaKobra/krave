// Placement for a transform node.
//
// These move everything UPSTREAM of the node as one unit, which is why they are
// here rather than in PropertyPanel's shared placement section: a transform is
// not a VisualNode and has no box of its own to place.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../../controls"

ColumnLayout {
    id: root

    /// The selected Node, or null.
    property var node: null
    /// Its typeId, hoisted so the visibility test costs no null check.
    property string nodeType: ""

    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "transform"

    SectionHeader { text: qsTr("Placement") }

    ParamSlider {
        label: qsTr("Offset X")
        info: qsTr("Moves everything feeding this node across the canvas, as a "
                   + "fraction of its width.")
        target: root.node; propertyName: "offsetX"
        from: -1; to: 1; origin: 0
    }
    ParamSlider {
        label: qsTr("Offset Y")
        info: qsTr("Moves everything feeding this node down the canvas, as a "
                   + "fraction of its height.")
        target: root.node; propertyName: "offsetY"
        from: -1; to: 1; origin: 0
    }
    ParamSlider {
        label: qsTr("Scale X")
        info: qsTr("Stretches the group horizontally about the pivot. 1 leaves "
                   + "it as it is.")
        target: root.node; propertyName: "scaleX"
        from: 0; to: 4; origin: 1
    }
    ParamSlider {
        label: qsTr("Scale Y")
        info: qsTr("Stretches the group vertically about the pivot. 1 leaves it "
                   + "as it is.")
        target: root.node; propertyName: "scaleY"
        from: 0; to: 4; origin: 1
    }
    ParamSlider {
        label: qsTr("Rotation")
        info: qsTr("Turns the group about the pivot. Negative goes "
                   + "anticlockwise.")
        target: root.node; propertyName: "rotation"
        from: -180; to: 180; stepSize: 1; unit: "°"; origin: 0
    }

    SectionHeader { text: qsTr("Pivot") }

    ParamSlider {
        label: qsTr("Pivot X")
        info: qsTr("The point the scale and rotation happen about, across the "
                   + "canvas: 0 is the left edge, 1 the right.")
        details: qsTr("The pivot is a point on the CANVAS, not on the group. "
                      + "Nothing in the graph knows how big the composited "
                      + "group is -- that is the same reason this node can move "
                      + "a whole subgraph without ever rendering it to a "
                      + "buffer.")
        target: root.node; propertyName: "pivotX"
        from: 0; to: 1
    }
    ParamSlider {
        label: qsTr("Pivot Y")
        info: qsTr("The point the scale and rotation happen about, down the "
                   + "canvas: 0 is the top, 1 the bottom.")
        target: root.node; propertyName: "pivotY"
        from: 0; to: 1
    }
}
