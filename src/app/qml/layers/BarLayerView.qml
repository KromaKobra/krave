// Frequency-reactive EQ bars.
//
// Every visual property is a live binding to the C++ BarLayer, so dragging a
// slider in the property panel updates the canvas on the next frame with no
// reload. Band values arrive from SpectrumEngine, which writes them into
// layerData.bands and fires bandsChanged.
//
// There are three layouts. A straight base line is plain Rectangles in a row.
// An arced base line and radial mode are the same fan of bars around a centre
// point, so they share FanBars; each bar rotates about a 1x1 pivot Item rather
// than reaching for trigonometry or QtQuick.Shapes -- plain Rectangles stay
// cheap and keep corner radius working. The one exception is "bars match arc",
// where a bar has to taper and therefore cannot be a Rectangle; that path uses
// a small Shape per bar and is off by default.
import QtQuick
import QtQuick.Shapes

LayerFrame {
    id: frame

    // The bar count comes from the data, not from a visual setting: the separator
    // decides how many bands there are, and mirroring doubles them.
    //
    // Zero bands is a REAL state now -- it is what an unconnected audio input
    // produces -- and it must draw nothing. `barCount` stays clamped to at least 1
    // because the geometry and colour maths below divide by it; `hasBands` is what
    // decides whether any of that gets drawn at all. Without the split, an
    // unconnected node renders one bar filling the whole layer, which reads as a
    // deliberate line rather than as a missing wire.
    readonly property int bandCount: layerData ? layerData.barCount : 0
    readonly property bool hasBands: bandCount > 0
    readonly property int barCount: Math.max(1, bandCount)
    readonly property string orientation: layerData ? layerData.orientation : "linear"
    readonly property bool mirrorBars: layerData ? layerData.mirrorBars : false
    readonly property real arc: layerData ? layerData.arc : 0
    /// Below this the radius of curvature runs away to infinity and the fan is
    /// indistinguishable from a straight line, so draw the straight one.
    readonly property bool curved: orientation !== "radial" && Math.abs(arc) >= 0.005

    function levelAt(index) {
        if (!layerData || !layerData.bands || index >= layerData.bands.length)
            return 0
        return layerData.bands[index]
    }

    /// Bands below the analysis window's lowest resolvable frequency all report
    /// the same value; drawing them merges them into one wide slab at the edge.
    /// Skipping them leaves a gap and keeps every drawn bar the same width.
    function barActive(index) {
        if (!layerData)
            return true
        const dead = layerData.inactiveEdgeBars
        if (dead <= 0)
            return true
        if (!layerData.mirrorSpectrum)
            return index >= dead
        // Mirrored: the dead bands follow the lowest bands, so they sit at both
        // outer edges normally and bunch around the seam when bass is inner.
        const half = barCount / 2
        return layerData.bassInner
            ? (index < half - dead || index >= half + dead)
            : (index >= dead && index < barCount - dead)
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

    // ------------------------------------------------------------- fan pieces
    // Both shapes below sit on a pivot that has already been rotated to the
    // bar's angle, so they only need the two radii the bar spans. `side` says
    // which way the base line lies from the centre of curvature: -1 when it is
    // above (a dome, and radial), +1 when it is below (a bowl).

    /// A constant-width bar. The default, and the cheap one.
    component FanBrick: Rectangle {
        property real nearR: 0
        property real farR: 0
        property int side: -1

        x: -width / 2
        y: Math.min(side * nearR, side * farR)
        height: Math.abs(farR - nearR)
    }

    /// A bar that tapers to fill its angular slot at every radius, so a fanned
    /// spectrum has no wedge gaps between the tips. A Rectangle cannot taper,
    /// hence the Shape; the curve renderer antialiases the slanted sides, which
    /// matters because these are only a few pixels wide.
    component FanWedge: Shape {
        id: wedge

        property real nearR: 0
        property real farR: 0
        property real nearW: 0
        property real farW: 0
        property int side: -1
        property color fill: "white"

        /// Which end of the bar lands at the top of the local bounding box.
        readonly property bool nearAtTop: side * nearR <= side * farR
        readonly property real topW: nearAtTop ? nearW : farW
        readonly property real bottomW: nearAtTop ? farW : nearW

        width: Math.max(nearW, farW)
        height: Math.abs(farR - nearR)
        x: -width / 2
        y: Math.min(side * nearR, side * farR)
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: -1
            fillColor: wedge.fill

            startX: (wedge.width - wedge.topW) / 2
            startY: 0
            PathLine { x: (wedge.width + wedge.topW) / 2; y: 0 }
            PathLine { x: (wedge.width + wedge.bottomW) / 2; y: wedge.height }
            PathLine { x: (wedge.width - wedge.bottomW) / 2; y: wedge.height }
        }
    }

    /// Bars standing on a circular base line, swept around a centre point.
    ///
    /// Rotating a 1x1 pivot at the centre swings its child bar around the circle
    /// without any trigonometry per bar. A pivot rotates clockwise, so a base
    /// line below the centre sweeps the opposite way and the caller passes a
    /// negative `angleStep` to keep bar 0 on the left.
    component FanBars: Item {
        id: fan

        /// The BarLayerView that owns this fan. Inline components cannot see the
        /// file's ids, so the helpers and the layer data come in through here.
        property var view: null
        property real centerX: 0
        property real centerY: 0
        /// Radius of the base line the bars stand on.
        property real radius: 0
        /// Degrees between neighbouring bars, and the angle of bar 0.
        property real angleStep: 0
        property real angleStart: 0
        /// Pixels a full-level bar covers, in one direction.
        property real extent: 0
        /// True when bars grow away from the centre, false when they close in.
        property bool outward: true
        /// Bars grow both ways from the base line.
        property bool mirrored: false
        /// Bars taper to fill their slot instead of staying constant-width.
        property bool matched: false
        property int side: -1

        readonly property int count: view ? view.barCount : 1
        readonly property real cornerRadius: view && view.layerData ? view.layerData.cornerRadius : 0
        readonly property real widthRatio: view && view.layerData ? view.layerData.barWidthRatio : 1
        /// The bar's angular slot, in radians.
        readonly property real slot: Math.abs(angleStep) * Math.PI / 180
        /// Arc length of one slot at the base line -- for a straight spectrum
        /// this is just the layer width over the bar count.
        readonly property real baseWidth: Math.max(1, radius * slot * widthRatio)

        /// Radius a bar's tip reaches. `away` picks the direction: the primary
        /// bar follows `outward`, the mirrored one goes the other way. The clamp
        /// stops a long bar from passing through the centre and folding over.
        function tipOf(len, away) {
            return away ? radius + len : Math.max(0, radius - len)
        }

        /// Slot width at a given radius, which is what a matched bar tapers to.
        function widthAt(r) {
            return Math.max(1, r * slot * widthRatio)
        }

        anchors.fill: parent

        // Setting a model to 0 destroys the delegates outright, so only one of
        // these two ever exists and the Shape path costs nothing when it is off.
        Repeater {
            model: fan.matched ? 0 : fan.count

            Item {
                required property int index

                readonly property real level: fan.view.levelAt(index)
                readonly property real len: fan.view.barLength(level, fan.extent)

                visible: fan.view.barActive(index)
                x: fan.centerX
                y: fan.centerY
                width: 1
                height: 1
                rotation: fan.angleStart + index * fan.angleStep

                FanBrick {
                    nearR: fan.radius
                    farR: fan.tipOf(parent.len, fan.outward)
                    side: fan.side
                    width: fan.baseWidth
                    radius: fan.cornerRadius
                    color: fan.view.barColor(parent.index, parent.level)
                }

                FanBrick {
                    visible: fan.mirrored
                    nearR: fan.radius
                    farR: fan.tipOf(parent.len, !fan.outward)
                    side: fan.side
                    width: fan.baseWidth
                    radius: fan.cornerRadius
                    color: fan.view.barColor(parent.index, parent.level)
                }
            }
        }

        Repeater {
            model: fan.matched ? fan.count : 0

            Item {
                required property int index

                readonly property real level: fan.view.levelAt(index)
                readonly property real len: fan.view.barLength(level, fan.extent)

                visible: fan.view.barActive(index)
                x: fan.centerX
                y: fan.centerY
                width: 1
                height: 1
                rotation: fan.angleStart + index * fan.angleStep

                FanWedge {
                    readonly property real tip: fan.tipOf(parent.len, fan.outward)

                    nearR: fan.radius
                    farR: tip
                    nearW: fan.baseWidth
                    farW: fan.widthAt(tip)
                    side: fan.side
                    fill: fan.view.barColor(parent.index, parent.level)
                }

                FanWedge {
                    readonly property real tip: fan.tipOf(parent.len, !fan.outward)

                    visible: fan.mirrored
                    nearR: fan.radius
                    farR: tip
                    nearW: fan.baseWidth
                    farW: fan.widthAt(tip)
                    side: fan.side
                    fill: fan.view.barColor(parent.index, parent.level)
                }
            }
        }
    }

    // ----------------------------------------------------------------- linear
    Item {
        anchors.fill: parent
        visible: frame.hasBands && frame.orientation !== "radial" && !frame.curved

        readonly property real slot: width / frame.barCount
        readonly property real barW: Math.max(1, slot * (frame.layerData ? frame.layerData.barWidthRatio : 1))
        readonly property bool mirrored: frame.mirrorBars
        readonly property bool up: frame.layerData ? frame.layerData.growUpward : true

        Repeater {
            model: frame.barCount

            Item {
                required property int index

                readonly property real level: frame.levelAt(index)
                readonly property real len: frame.barLength(level, parent.height * (parent.mirrored ? 0.5 : 1.0))

                visible: frame.barActive(index)
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

                // Lower half, mirrored only.
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

    // ------------------------------------------------------------ linear, arced
    Item {
        id: arced

        anchors.fill: parent
        visible: frame.hasBands && frame.curved

        // A circular base line whose two ends stay pinned to the box corners:
        // the chord is the full width and the middle bows away from it by the
        // sagitta. theta is the angle the whole spectrum subtends -- half a turn
        // at |arc| = 1.
        readonly property real theta: Math.abs(frame.arc) * Math.PI
        readonly property bool dome: frame.arc > 0
        readonly property real curveRadius: width / (2 * Math.sin(theta / 2))
        readonly property real sagitta: curveRadius * (1 - Math.cos(theta / 2))
        readonly property bool up: frame.layerData ? frame.layerData.growUpward : true
        readonly property real baseY: up ? height : 0
        readonly property real step: (dome ? 1 : -1) * theta * 180 / Math.PI / frame.barCount

        FanBars {
            view: frame
            centerX: arced.width / 2
            // The centre of curvature sits on the far side of the base line from
            // the bow, which is what makes a positive arc dome upward.
            centerY: arced.baseY + (arced.dome ? 1 : -1) * (arced.curveRadius - arced.sagitta)
            radius: arced.curveRadius
            side: arced.dome ? -1 : 1
            angleStep: arced.step
            angleStart: -arced.step * (frame.barCount - 1) / 2
            // Bars run away from the centre of curvature only when the bow and
            // the growth direction agree; otherwise they close in on it.
            outward: arced.dome === arced.up
            extent: arced.height * (frame.mirrorBars ? 0.5 : 1.0)
            mirrored: frame.mirrorBars
            matched: frame.layerData ? frame.layerData.barsMatchArc : false
        }
    }

    // ------------------------------------------------------------------- radial
    Item {
        id: radial

        anchors.fill: parent
        visible: frame.hasBands && frame.orientation === "radial"

        readonly property real minSide: Math.min(width, height)
        readonly property real inner: minSide * 0.5 * (frame.layerData ? frame.layerData.innerRadius : 0.35)
        readonly property real available: minSide * 0.5 - inner

        FanBars {
            view: frame
            centerX: radial.width / 2
            centerY: radial.height / 2
            radius: Math.max(1, radial.inner)
            side: -1
            angleStep: 360 / frame.barCount
            angleStart: 0
            outward: true
            // Mirrored bars reach into the hole as well, so the length has to
            // fit on both sides of the ring.
            extent: frame.mirrorBars ? Math.min(radial.available, radial.inner)
                                     : radial.available
            mirrored: frame.mirrorBars
        }
    }
}
