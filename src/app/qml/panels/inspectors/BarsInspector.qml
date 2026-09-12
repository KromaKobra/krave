// Shape and colour controls for a bar layer.
//
// One of the per-type blocks PropertyPanel stacks up. It stays instantiated and
// hides itself -- see the header comment there for why -- which is also why every
// binding tolerates being pointed at a layer that has no such property.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../../controls"

ColumnLayout {
    id: root

    /// The selected Layer, or null.
    property var node: null
    /// Its typeId, hoisted so the visibility test costs no null check.
    property string nodeType: ""

    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "bars"

    SectionHeader { text: qsTr("Shape") }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.textGhost
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeKicker
        text: root.node && root.node.mirrorSpectrum
              ? qsTr("%1 bars — the audio layer's bands, reflected")
                  .arg(root.node.barCount)
              : qsTr("%1 bars, set by the audio layer's band count")
                  .arg(root.node ? root.node.barCount : 0)
    }

    SegmentedControl {
        label: qsTr("Orientation")
        info: qsTr("Whether the bars stand along a line or radiate out from a "
                   + "ring in the middle.")
        target: root.node; propertyName: "orientation"
        options: [
            { text: qsTr("Linear"), value: "linear" },
            { text: qsTr("Radial"), value: "radial" }
        ]
    }
    ToggleRow {
        label: qsTr("Mirror bars")
        info: qsTr("Each bar grows both ways from its base line instead of "
                   + "only into the layer.")
        details: qsTr("This is a vertical mirror, and it works in either "
                      + "orientation: linear bars grow up and down from the "
                      + "centre line, radial bars grow outward and inward from "
                      + "the inner ring.\n\n"
                      + "It is not the same thing as Mirror spectrum, which "
                      + "reflects the frequency axis instead. The two are "
                      + "independent and are often used together.")
        target: root.node
        propertyName: "mirrorBars"
    }
    ParamSlider {
        visible: root.node && root.node.orientation === "radial"
        label: qsTr("Inner radius")
        info: qsTr("Radial only: how big the hole in the middle is, as a "
                   + "fraction of the layer's shorter side.")
        target: root.node
        propertyName: "innerRadius"
        from: 0.05; to: 0.9
    }
    ParamSlider {
        visible: root.node && root.node.orientation !== "radial"
        label: qsTr("Arc")
        info: qsTr("Bends the base line. 0 is straight, positive bows it up "
                   + "into a dome, negative down into a bowl.")
        details: qsTr("The bars keep their order along the line; only the line "
                      + "itself curves. The arc spans |value| × 180 degrees, so "
                      + "0.5 is a quarter turn and 1 bends the row into a "
                      + "complete half circle.\n\n"
                      + "Wide arcs leave wedge-shaped gaps between bars, "
                      + "because each bar keeps a constant width while its slot "
                      + "widens with radius. Bars match arc fixes that.")
        target: root.node; propertyName: "arc"
        from: -1; to: 1; origin: 0
    }
    ToggleRow {
        visible: root.node && root.node.orientation !== "radial"
                 && root.node.arc !== 0
        sub: true
        label: qsTr("Bars match arc")
        info: qsTr("Bars taper to fill their slot at every radius, closing the "
                   + "wedge gaps. Corner radius is ignored.")
        target: root.node
        propertyName: "barsMatchArc"
    }
    ParamSlider {
        label: qsTr("Bar width")
        info: qsTr("How much of its slot each bar fills. Lower leaves wider "
                   + "gaps between them.")
        target: root.node
        propertyName: "barWidthRatio"
        from: 0.05; to: 1
    }
    ParamSlider {
        label: qsTr("Corner radius")
        info: qsTr("Rounds the ends of every bar.")
        target: root.node
        propertyName: "cornerRadius"
        from: 0; to: 24; stepSize: 0.5; unit: "px"
    }
    ParamSlider {
        label: qsTr("Min height")
        info: qsTr("The height a bar keeps when its band is silent, so the row "
                   + "never disappears completely.")
        target: root.node; propertyName: "minHeight"
        from: 0; to: 0.5; stepSize: 0.001
    }
    ParamSlider {
        label: qsTr("Max height")
        info: qsTr("How tall a bar at full level reaches, as a fraction of the "
                   + "layer's height.")
        target: root.node; propertyName: "maxHeight"
        from: 0.05; to: 1
    }

    ToggleRow {
        label: qsTr("Mirror spectrum")
        info: qsTr("Repeats the band order back on itself, so the spectrum "
                   + "reads symmetrically.")
        details: qsTr("This reflects the frequency axis, not the bars: the "
                      + "same bands appear twice, once in each direction. It is "
                      + "what gives the symmetric skyline look, and it doubles "
                      + "the number of bars drawn.\n\n"
                      + "Independent of both Orientation and Mirror bars — on a "
                      + "radial layer it makes the ring symmetric about its "
                      + "vertical axis.")
        target: root.node
        propertyName: "mirrorSpectrum"
    }
    ToggleRow {
        // A layer of another type has no such property at all, so this reads
        // `undefined` unless it is coerced.
        visible: root.node ? root.node.mirrorSpectrum === true : false
        sub: true
        label: qsTr("Bass in the centre")
        info: qsTr("Which end of the mirrored spectrum meets in the middle. "
                   + "Off puts the lowest bands at both outer edges instead.")
        target: root.node
        propertyName: "bassInner"
    }

    SectionHeader { text: qsTr("Colour") }

    SegmentedControl {
        label: qsTr("Mode"); target: root.node; propertyName: "colorMode"
        info: qsTr("Solid paints every bar Colour A; Gradient blends A into B "
                   + "across the row; Spectrum sweeps a hue across it.")
        options: [
            { text: qsTr("Solid"), value: "solid" },
            { text: qsTr("Gradient"), value: "gradient" },
            { text: qsTr("Spectrum"), value: "spectrum" }
        ]
    }
    ColorField {
        label: qsTr("Colour A"); target: root.node; propertyName: "colorA"
        info: qsTr("The bar colour, or the low-frequency end of the gradient.")
        visible: root.node && root.node.colorMode !== "spectrum"
    }
    ColorField {
        label: qsTr("Colour B"); target: root.node; propertyName: "colorB"
        info: qsTr("The high-frequency end of the gradient.")
        visible: root.node && root.node.colorMode === "gradient"
    }
    ParamSlider {
        label: qsTr("Reactive brightness")
        info: qsTr("Extra brightness given to each bar in proportion to its "
                   + "level. At 0 the colour ignores the audio entirely.")
        target: root.node
        propertyName: "reactiveBrightness"
        from: 0; to: 2
    }

    Text {
        Layout.fillWidth: true
        Layout.topMargin: Theme.space2
        wrapMode: Text.WordWrap
        color: Theme.textGhost
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeKicker
        text: qsTr("Frequency response, smoothing and dynamics live on the "
                   + "Audio layer that feeds this one.")
    }
}
