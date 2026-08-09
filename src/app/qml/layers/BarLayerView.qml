// Frequency-reactive EQ bars.
//
// Every visual property is a live binding to the C++ BarLayer, so dragging a
// slider in the property panel updates the canvas on the next frame with no
// reload. Band values arrive from SpectrumEngine, which writes them into
// layerData.bands and fires bandsChanged.
//
// Radial mode rotates each bar about the layer centre with a 1x1 pivot Item
// rather than reaching for QtQuick.Shapes -- plain Rectangles stay cheap and
// keep corner radius working.
import QtQuick

LayerFrame {
    id: frame

    readonly property int barCount: layerData ? Math.max(1, layerData.barCount) : 1
    readonly property string orientation: layerData ? layerData.orientation : "linear"

    function levelAt(index) {
        if (!layerData || !layerData.bands || index >= layerData.bands.length)
            return 0
        return layerData.bands[index]
    }

    function mixColor(a, b, t) {
        return Qt.rgba(a.r + (b.r - a.r) * t,
                       a.g + (b.g - a.g) * t,
                       a.b + (b.b - a.b) * t,
                       a.a + (b.a - a.a) * t)
    }

    function barColor(index, level) {
        if (!layerData)
            return "white"
        const t = barCount > 1 ? index / (barCount - 1) : 0
        let base
        switch (layerData.colorMode) {
        case "solid":
            base = layerData.colorA
            break
        case "spectrum":
            base = Qt.hsva(t, 0.85, 1.0, 1.0)
            break
        default:
            base = mixColor(layerData.colorA, layerData.colorB, t)
            break
        }
        if (layerData.reactiveBrightness > 0)
            base = Qt.lighter(base, 1.0 + level * layerData.reactiveBrightness)
        return base
    }

    /// Bar length in pixels for the given level, as a fraction of `extent`.
    function barLength(level, extent) {
        const lo = layerData ? layerData.minHeight : 0
        const hi = layerData ? layerData.maxHeight : 1
        return Math.max(1, (lo + (hi - lo) * level) * extent)
    }

    // ------------------------------------------------------------ linear / mirrored
    Item {
        anchors.fill: parent
        visible: frame.orientation !== "radial"

        readonly property real slot: width / frame.barCount
        readonly property real barW: Math.max(1, slot * (frame.layerData ? frame.layerData.barWidthRatio : 1))
        readonly property bool mirrored: frame.orientation === "mirrored"
        readonly property bool up: frame.layerData ? frame.layerData.growUpward : true

        Repeater {
            model: frame.barCount

            Item {
                required property int index

                readonly property real level: frame.levelAt(index)
                readonly property real len: frame.barLength(level, parent.height * (parent.mirrored ? 0.5 : 1.0))

                x: parent.slot * index + (parent.slot - parent.barW) / 2
                width: parent.barW
                height: parent.height

                // Upper half (or the whole bar when not mirrored).
                Rectangle {
                    width: parent.width
                    height: parent.len
                    radius: frame.layerData ? frame.layerData.cornerRadius : 0
                    color: frame.barColor(parent.index, parent.level)
                    y: parent.parent.mirrored
                       ? parent.height / 2 - height
                       : (parent.parent.up ? parent.height - height : 0)
                }

                // Lower half, mirrored mode only.
                Rectangle {
                    visible: parent.parent.mirrored
                    width: parent.width
                    height: parent.len
                    radius: frame.layerData ? frame.layerData.cornerRadius : 0
                    color: frame.barColor(parent.index, parent.level)
                    y: parent.height / 2
                }
            }
        }
    }

    // ------------------------------------------------------------------- radial
    Item {
        id: radial
        anchors.fill: parent
        visible: frame.orientation === "radial"

        readonly property real minSide: Math.min(width, height)
        readonly property real inner: minSide * 0.5 * (frame.layerData ? frame.layerData.innerRadius : 0.35)
        readonly property real available: minSide * 0.5 - inner
        readonly property real circumference: 2 * Math.PI * Math.max(1, inner)
        readonly property real barW: Math.max(1, (circumference / frame.barCount)
                                              * (frame.layerData ? frame.layerData.barWidthRatio : 1))

        Repeater {
            model: frame.barCount

            // 1x1 pivot sitting at the centre; rotating it swings the child bar
            // around the circle without any trigonometry per bar.
            Item {
                required property int index

                readonly property real level: frame.levelAt(index)

                x: radial.width / 2
                y: radial.height / 2
                width: 1
                height: 1
                rotation: index * 360 / frame.barCount

                Rectangle {
                    width: radial.barW
                    height: frame.barLength(parent.level, radial.available)
                    x: -width / 2
                    y: -(radial.inner + height)
                    radius: frame.layerData ? frame.layerData.cornerRadius : 0
                    color: frame.barColor(parent.index, parent.level)
                }
            }
        }
    }
}
