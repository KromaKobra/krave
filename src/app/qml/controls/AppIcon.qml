import QtQuick
import QtQuick.Shapes

import MusicVisualizer

// Renders one path from Icons at any size, in any colour.
//
// The path data is authored on a 24x24 grid and scaled here. CurveRenderer keeps
// it resolution-independent, so the same declaration is crisp at 14px in a row
// and at 26px in the brand mark.
Item {
    id: root

    /// SVG path data, normally an `Icons.*` property.
    property string path: ""
    property color color: Theme.text
    /// Edge length in pixels; the icon is always square.
    property real size: 16
    /// Paint the path instead of stroking it. For solid marks like the play glyph.
    property bool filled: false
    /// Stroke weight on the 24-unit grid, so it scales with `size`.
    property real weight: 1.9

    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: root.size / 24
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.filled ? "transparent" : root.color
            fillColor: root.filled ? root.color : "transparent"
            strokeWidth: root.weight
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: root.path }
        }
    }
}
