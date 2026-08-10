// Editor for the selected layer.
//
// Every control writes directly to a Q_PROPERTY on the C++ Layer object. Because
// the canvas binds to those same properties, edits are visible on the next frame
// -- there is no apply step and no reload.
//
// The sections for every layer type stay instantiated and hide themselves, which
// is why each control tolerates being pointed at a layer that has no such
// property: the alternative, tearing the panel down on every selection change,
// costs more than the handful of idle bindings does.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import krave
import "../controls"

Rectangle {
    id: root

    readonly property var selected: layerModel.selectedLayer
    readonly property string selectedType: selected ? selected.typeId : ""

    color: Theme.surface

    Rectangle {
        width: 1
        height: parent.height
        color: Theme.divider
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------------- header
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space6
                anchors.rightMargin: Theme.space6
                spacing: Theme.space3

                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: Theme.radiusSm
                    color: Theme.withAlpha(Theme.accent, 0.16)
                    visible: root.selected !== null

                    AppIcon {
                        anchors.centerIn: parent
                        path: LayerTypes.icon(root.selectedType)
                        size: 16
                        color: Theme.accent
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: root.selected ? root.selected.name : qsTr("Nothing selected")
                        color: root.selected ? Theme.text : Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Theme.weightMedium
                        elide: Text.ElideRight
                    }

                    Tag {
                        visible: root.selected !== null
                        variant: "accent"
                        text: LayerTypes.label(root.selectedType)
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.divider
            }
        }

        // -------------------------------------------------------- empty state
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.selected === null

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - Theme.space8 * 2
                spacing: Theme.space3

                AppIcon {
                    Layout.alignment: Qt.AlignHCenter
                    path: Icons.sliders
                    size: 26
                    color: Theme.textGhost
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Select a layer to edit its properties.")
                    color: Theme.textGhost
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ---------------------------------------------------------- properties
        Flickable {
            id: scroller

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.selected !== null

            contentWidth: width
            contentHeight: column.implicitHeight + Theme.space8
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: AppScrollBar {}

            ColumnLayout {
                id: column

                width: scroller.width - Theme.space6 * 2
                x: Theme.space6
                spacing: Theme.space4

                // ------------------------------------------------------ layer
                SectionHeader { text: qsTr("Layer"); leading: true }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    ParamLabel {
                        text: qsTr("Name")
                        info: qsTr("What this layer is called in the layer list. It has no "
                                   + "effect on what gets drawn.")
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        text: root.selected ? root.selected.name : ""
                        onTextEdited: if (root.selected) root.selected.name = text
                    }
                }

                ToggleRow {
                    label: qsTr("Visible")
                    info: qsTr("Hides the layer without deleting it. Hidden layers are left "
                               + "out of the export too.")
                    target: root.selected
                    propertyName: "layerVisible"
                }

                ParamSlider {
                    label: qsTr("Opacity")
                    info: qsTr("How much of the layer shows through: invisible at 0, solid at 1.")
                    target: root.selected; propertyName: "layerOpacity"
                    from: 0; to: 1
                }

                SegmentedControl {
                    label: qsTr("Blend mode"); target: root.selected; propertyName: "blendMode"
                    info: qsTr("How this layer's colour combines with whatever is beneath it.")
                    details: qsTr("Normal simply covers what is underneath, using the layer's "
                                  + "opacity to mix.\n\n"
                                  + "Add sums the two colours. Overlapping bright areas climb "
                                  + "toward white, which is what makes bars over a dark "
                                  + "background read as glowing rather than as flat shapes. "
                                  + "Nothing can get darker than what was already there.\n\n"
                                  + "Screen is a gentler version of the same idea: it "
                                  + "brightens, but rolls off as it approaches white instead "
                                  + "of clipping there. Reach for it when Add blows out the "
                                  + "peaks.")
                    options: [
                        { text: qsTr("Normal"), value: "normal" },
                        { text: qsTr("Add"), value: "add" },
                        { text: qsTr("Screen"), value: "screen" }
                    ]
                }

                // -------------------------------------------------- placement
                SectionHeader { text: qsTr("Placement") }

                ParamSlider {
                    label: qsTr("Centre X")
                    info: qsTr("Where the layer's centre sits across the canvas: 0 is the left "
                               + "edge, 1 the right.")
                    target: root.selected; propertyName: "posX"
                    from: -0.5; to: 1.5
                }
                ParamSlider {
                    label: qsTr("Centre Y")
                    info: qsTr("Where the layer's centre sits down the canvas: 0 is the top, "
                               + "1 the bottom.")
                    target: root.selected; propertyName: "posY"
                    from: -0.5; to: 1.5
                }
                ParamSlider {
                    label: qsTr("Width")
                    info: qsTr("Layer width as a fraction of the canvas, so 1 spans it "
                               + "completely and the framing survives a change of resolution.")
                    target: root.selected; propertyName: "sizeW"
                    from: 0; to: 2
                }
                ParamSlider {
                    label: qsTr("Height")
                    info: qsTr("Layer height as a fraction of the canvas, so 1 spans it "
                               + "completely and the framing survives a change of resolution.")
                    target: root.selected; propertyName: "sizeH"
                    from: 0; to: 2
                }
                ParamSlider {
                    label: qsTr("Rotation")
                    info: qsTr("Turns the layer about its own centre. Negative goes "
                               + "anticlockwise.")
                    target: root.selected; propertyName: "layerRotation"
                    from: -180; to: 180; stepSize: 1; unit: "°"
                }

                // ------------------------------------------------------- bars
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    visible: root.selectedType === "bars"

                    SectionHeader { text: qsTr("Shape") }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textGhost
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeKicker
                        text: root.selected && root.selected.mirrorSpectrum
                              ? qsTr("%1 bars — the audio layer's bands, reflected")
                                  .arg(root.selected.barCount)
                              : qsTr("%1 bars, set by the audio layer's band count")
                                  .arg(root.selected ? root.selected.barCount : 0)
                    }

                    SegmentedControl {
                        label: qsTr("Orientation")
                        info: qsTr("Whether the bars stand along a line or radiate out from a "
                                   + "ring in the middle.")
                        target: root.selected; propertyName: "orientation"
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
                        target: root.selected
                        propertyName: "mirrorBars"
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.orientation === "radial"
                        label: qsTr("Inner radius")
                        info: qsTr("Radial only: how big the hole in the middle is, as a "
                                   + "fraction of the layer's shorter side.")
                        target: root.selected
                        propertyName: "innerRadius"
                        from: 0.05; to: 0.9
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.orientation !== "radial"
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
                        target: root.selected; propertyName: "arc"
                        from: -1; to: 1; origin: 0
                    }
                    ToggleRow {
                        visible: root.selected && root.selected.orientation !== "radial"
                                 && root.selected.arc !== 0
                        sub: true
                        label: qsTr("Bars match arc")
                        info: qsTr("Bars taper to fill their slot at every radius, closing the "
                                   + "wedge gaps. Corner radius is ignored.")
                        target: root.selected
                        propertyName: "barsMatchArc"
                    }
                    ParamSlider {
                        label: qsTr("Bar width")
                        info: qsTr("How much of its slot each bar fills. Lower leaves wider "
                                   + "gaps between them.")
                        target: root.selected
                        propertyName: "barWidthRatio"
                        from: 0.05; to: 1
                    }
                    ParamSlider {
                        label: qsTr("Corner radius")
                        info: qsTr("Rounds the ends of every bar.")
                        target: root.selected
                        propertyName: "cornerRadius"
                        from: 0; to: 24; stepSize: 0.5; unit: "px"
                    }
                    ParamSlider {
                        label: qsTr("Min height")
                        info: qsTr("The height a bar keeps when its band is silent, so the row "
                                   + "never disappears completely.")
                        target: root.selected; propertyName: "minHeight"
                        from: 0; to: 0.5; stepSize: 0.001
                    }
                    ParamSlider {
                        label: qsTr("Max height")
                        info: qsTr("How tall a bar at full level reaches, as a fraction of the "
                                   + "layer's height.")
                        target: root.selected; propertyName: "maxHeight"
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
                        target: root.selected
                        propertyName: "mirrorSpectrum"
                    }
                    ToggleRow {
                        // A layer of another type has no such property at all, so
                        // this reads `undefined` unless it is coerced.
                        visible: root.selected ? root.selected.mirrorSpectrum === true : false
                        sub: true
                        label: qsTr("Bass in the centre")
                        info: qsTr("Which end of the mirrored spectrum meets in the middle. "
                                   + "Off puts the lowest bands at both outer edges instead.")
                        target: root.selected
                        propertyName: "bassInner"
                    }

                    SectionHeader { text: qsTr("Colour") }

                    SegmentedControl {
                        label: qsTr("Mode"); target: root.selected; propertyName: "colorMode"
                        info: qsTr("Solid paints every bar Colour A; Gradient blends A into B "
                                   + "across the row; Spectrum sweeps a hue across it.")
                        options: [
                            { text: qsTr("Solid"), value: "solid" },
                            { text: qsTr("Gradient"), value: "gradient" },
                            { text: qsTr("Spectrum"), value: "spectrum" }
                        ]
                    }
                    ColorField {
                        label: qsTr("Colour A"); target: root.selected; propertyName: "colorA"
                        info: qsTr("The bar colour, or the low-frequency end of the gradient.")
                        visible: root.selected && root.selected.colorMode !== "spectrum"
                    }
                    ColorField {
                        label: qsTr("Colour B"); target: root.selected; propertyName: "colorB"
                        info: qsTr("The high-frequency end of the gradient.")
                        visible: root.selected && root.selected.colorMode === "gradient"
                    }
                    ParamSlider {
                        label: qsTr("Reactive brightness")
                        info: qsTr("Extra brightness given to each bar in proportion to its "
                                   + "level. At 0 the colour ignores the audio entirely.")
                        target: root.selected
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

                // ---------------------------------------------- audio provider
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    visible: root.selectedType === "audio"

                    SectionHeader { text: qsTr("Resolution") }

                    ParamSlider {
                        label: qsTr("Band count")
                        info: qsTr("How many bands the spectrum is split into. Every bar layer "
                                   + "fed by this one draws this many bars.")
                        target: root.selected; propertyName: "bandCount"
                        from: 4; to: 256; stepSize: 1
                    }
                    ParamSlider {
                        label: qsTr("Window")
                        info: qsTr("How much audio each measurement looks at. Longer separates "
                                   + "close frequencies but responds more slowly.")
                        details: qsTr("Frequency resolution and time resolution trade against "
                                      + "each other, and this is the dial between them. A long "
                                      + "window can tell two neighbouring partials apart but "
                                      + "smears a fast transient across several frames; a short "
                                      + "one catches the transient and blurs the pitches "
                                      + "together.\n\n"
                                      + "Percussive material wants a short window, sustained "
                                      + "harmonic material a long one. The value is rounded up "
                                      + "to a power of two internally, so nearby settings can "
                                      + "behave identically.")
                        target: root.selected; propertyName: "windowMs"
                        from: 10; to: 400; stepSize: 5; unit: "ms"
                    }
                    ParamSlider {
                        label: qsTr("Windows averaged")
                        info: qsTr("How many overlapping measurements are averaged per frame. "
                                   + "Higher is steadier.")
                        details: qsTr("A single FFT is a high-variance estimate: the floor "
                                      + "between partials comes out looking like grass, and it "
                                      + "moves every frame even on a steady tone.\n\n"
                                      + "Averaging several overlapping windows cuts that noise "
                                      + "and leaves the real peaks standing. It costs a little "
                                      + "responsiveness — the average necessarily includes "
                                      + "slightly older audio — but far less than lengthening "
                                      + "the window would.")
                        target: root.selected
                        propertyName: "averageCount"
                        from: 1; to: 16; stepSize: 1
                    }

                    SectionHeader { text: qsTr("Frequency") }

                    ParamSlider {
                        label: qsTr("Low cut")
                        info: qsTr("The lowest frequency given a band. Everything below it is "
                                   + "left out of the display.")
                        target: root.selected; propertyName: "minHz"
                        from: 10; to: 500; stepSize: 5; unit: "Hz"
                    }
                    ParamSlider {
                        label: qsTr("High cut")
                        info: qsTr("The highest frequency given a band. Most music has little "
                                   + "left above 16 kHz.")
                        target: root.selected; propertyName: "maxHz"
                        from: 200; to: 22000; stepSize: 100; unit: "Hz"
                    }
                    ParamSlider {
                        label: qsTr("Linear ←→ log spacing")
                        info: qsTr("How the frequency range is divided between the bands: 0 is "
                                   + "logarithmic, 1 is linear.")
                        details: qsTr("Logarithmic spacing gives each octave the same number of "
                                      + "bands, which is how hearing works and what makes the "
                                      + "whole range read evenly. It is the right default.\n\n"
                                      + "Linear spacing gives each equal slice of hertz the "
                                      + "same number of bands, so almost everything piles into "
                                      + "the treble. Its use is over a narrow low range: set a "
                                      + "low cut and a high cut a few hundred hertz apart and "
                                      + "turn this up, and a bass line's harmonics resolve into "
                                      + "separate spikes instead of one lump.")
                        target: root.selected
                        propertyName: "hzLinearFactor"
                        from: 0; to: 1
                    }

                    SectionHeader { text: qsTr("Response") }

                    SegmentedControl {
                        label: qsTr("Amplitude"); target: root.selected
                        propertyName: "amplitudeMode"
                        info: qsTr("Whether loudness is read as a decibel window or as raw "
                                   + "magnitude times a gain.")
                        details: qsTr("Decibel maps a floor-to-ceiling window onto the full "
                                      + "height of a bar. Because hearing is logarithmic, this "
                                      + "lifts everything: quiet detail becomes visible and the "
                                      + "spectrum reads as one continuous mound.\n\n"
                                      + "Linear draws the magnitude itself, scaled by Gain. It "
                                      + "leaves a flat floor with sharp peaks standing on it, "
                                      + "which is the classic spectrum-analyser look, at the "
                                      + "cost of showing almost nothing between the peaks.")
                        options: [
                            { text: qsTr("Decibel"), value: "db" },
                            { text: qsTr("Linear"), value: "linear" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.amplitudeMode === "linear"
                        label: qsTr("Gain")
                        info: qsTr("Linear mode only: multiplies the raw magnitude before it "
                                   + "is drawn.")
                        target: root.selected; propertyName: "linearGain"
                        from: 0.5; to: 60; stepSize: 0.5
                    }
                    ParamSlider {
                        label: qsTr("Low-frequency level")
                        info: qsTr("Gain at the bottom of the range, ramping to full at the "
                                   + "top. Lower it to stop bass dominating.")
                        details: qsTr("Bass carries most of the energy in almost any mix, so "
                                      + "an honest readout piles the whole picture up at the "
                                      + "low end and leaves the rest of the width nearly "
                                      + "idle.\n\n"
                                      + "Pulling the bottom down spreads the display across all "
                                      + "of its bars. The reduction applies at the low cut and "
                                      + "ramps smoothly back to no change at the high cut, so "
                                      + "it tilts the picture rather than carving a step into "
                                      + "it.")
                        target: root.selected
                        propertyName: "lowFreqMagnitude"
                        from: 0; to: 1
                    }
                    SegmentedControl {
                        label: qsTr("Band measure"); target: root.selected
                        propertyName: "bandMeasure"
                        info: qsTr("Whether a band reports the average of the frequencies it "
                                   + "covers or the loudest one in it.")
                        details: qsTr("Average is the steadier reading: a band's value moves "
                                      + "only when its whole slice of the spectrum moves.\n\n"
                                      + "Peak follows the single loudest frequency in the band "
                                      + "instead. It tracks individual partials, so it is more "
                                      + "responsive and considerably more revealing about what "
                                      + "is actually in the mix — and it is also the main cause "
                                      + "of frame-to-frame jitter, since a partial drifting "
                                      + "across a band boundary makes both bands jump. If Peak "
                                      + "looks too twitchy, raise Windows averaged or Neighbour "
                                      + "smoothing rather than going back to Average.")
                        options: [
                            { text: qsTr("Average"), value: "rms" },
                            { text: qsTr("Peak"), value: "peak" }
                        ]
                    }
                    ParamSlider {
                        label: qsTr("Treble lift")
                        info: qsTr("Lifts the high end to compensate for music's natural "
                                   + "roll-off, so the outer bars stay active.")
                        details: qsTr("Recorded music loses energy steadily as frequency "
                                      + "rises, which leaves the treble bars sitting on the "
                                      + "floor no matter how bright the track sounds.\n\n"
                                      + "This applies a tilt of so many decibels per octave "
                                      + "across the range, countering that slope. It is a "
                                      + "fixed correction, unlike Per-band auto-gain, so the "
                                      + "relative levels within the treble are preserved.")
                        target: root.selected
                        propertyName: "tiltDbPerOctave"
                        from: 0; to: 9; stepSize: 0.25; unit: "dB"
                    }
                    ParamSlider {
                        label: qsTr("Per-band auto-gain")
                        info: qsTr("References each band to its own recent average, so quiet "
                                   + "bands stay as animated as loud ones.")
                        details: qsTr("Each band tracks its own slow-moving average level and "
                                      + "is drawn relative to that, rather than against one "
                                      + "scale shared by the whole spectrum. A band that is "
                                      + "quiet all the time still uses its full height when "
                                      + "something happens in it.\n\n"
                                      + "The effect is that everything moves, everywhere, all "
                                      + "the time — which is either exactly what a visualizer "
                                      + "wants or a complete misrepresentation of the mix, "
                                      + "depending on the piece. Blend it in with the slider "
                                      + "rather than treating it as a switch, and use Auto-gain "
                                      + "adapt to set how quickly each band forgets.")
                        target: root.selected
                        propertyName: "bandNormalization"
                        from: 0; to: 1
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.bandNormalization > 0
                        label: qsTr("Auto-gain adapt")
                        info: qsTr("How long the auto-gain takes to follow a change in level. "
                                   + "Longer is steadier and less prone to pumping.")
                        target: root.selected
                        propertyName: "normalizationTau"
                        from: 0.2; to: 10; stepSize: 0.1; unit: "s"
                    }
                    ParamSlider {
                        label: qsTr("Floor")
                        info: qsTr("The level treated as silence. Anything at or below it sits "
                                   + "at the bottom of the bar.")
                        target: root.selected; propertyName: "floorDb"
                        from: -100; to: -20; stepSize: 1; unit: "dB"
                    }
                    ParamSlider {
                        label: qsTr("Ceiling")
                        info: qsTr("The level treated as full scale. Anything at or above it "
                                   + "reaches full height.")
                        target: root.selected; propertyName: "ceilingDb"
                        from: -40; to: 6; stepSize: 1; unit: "dB"
                    }
                    ParamSlider {
                        label: qsTr("Gate")
                        info: qsTr("How far above the floor a band must climb before it is "
                                   + "drawn at all. Larger empties the gaps between hits.")
                        details: qsTr("Every recording has a bed of low-level noise and room "
                                      + "tone under it. With a generous floor that bed is "
                                      + "visible, and the display never quite rests.\n\n"
                                      + "The gate fades a band out as it approaches the floor "
                                      + "instead of cutting at a threshold, so the transition "
                                      + "is smooth. Widening it makes hits land in silence and "
                                      + "read as more percussive; too wide and quiet passages "
                                      + "vanish entirely.")
                        target: root.selected; propertyName: "gateDb"
                        from: 0; to: 24; stepSize: 1; unit: "dB"
                    }
                    SegmentedControl {
                        label: qsTr("Scaling curve"); target: root.selected
                        propertyName: "scalingCurve"
                        info: qsTr("The curve applied between the floor and the ceiling, which "
                                   + "decides how much height the middle levels get.")
                        details: qsTr("Linear divides the height evenly across the "
                                      + "floor-to-ceiling window: honest, and usually the least "
                                      + "interesting to watch, because most of the music lives "
                                      + "in the bottom third of it.\n\n"
                                      + "Power bends that with an exponent you set through "
                                      + "Gamma, which is the flexible choice — below 1 lifts "
                                      + "quiet detail, above 1 flattens it and leaves only the "
                                      + "peaks moving.\n\n"
                                      + "Log compresses the top of the range hard and expands "
                                      + "the bottom, so quiet passages stay lively at the cost "
                                      + "of the loud ones all looking alike.")
                        options: [
                            { text: qsTr("Power"), value: "power" },
                            { text: qsTr("Linear"), value: "linear" },
                            { text: qsTr("Log"), value: "log" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.scalingCurve === "power"
                        label: qsTr("Gamma")
                        info: qsTr("The power curve's exponent: below 1 lifts quiet levels, "
                                   + "above 1 pushes them down.")
                        target: root.selected; propertyName: "gamma"
                        from: 0.2; to: 4
                    }

                    SectionHeader { text: qsTr("Motion") }

                    EnumCombo {
                        label: qsTr("Smoothing"); target: root.selected
                        propertyName: "smoothingMode"
                        info: qsTr("Whether bars rise and fall at the same rate, or rise fast "
                                   + "and fall slowly.")
                        details: qsTr("Symmetric uses one time constant in both directions. "
                                      + "Bars ease into their new level and ease back out of "
                                      + "it, and because each one is chasing a different "
                                      + "signal they never fall in step with each other.\n\n"
                                      + "Attack / decay splits the two. A fast attack with a "
                                      + "slow decay is the classic meter behaviour: hits land "
                                      + "instantly and then drain away. It reads as punchier, "
                                      + "with the caveat that every bar falls at an identical "
                                      + "mechanical rate, which can look like machinery rather "
                                      + "than music.")
                        options: [
                            { text: qsTr("Symmetric (natural)"), value: "symmetric" },
                            { text: qsTr("Attack / decay (punchy)"), value: "attackDecay" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.smoothingMode === "symmetric"
                        label: qsTr("Smoothing")
                        info: qsTr("How long a bar takes to follow a change, in both "
                                   + "directions. 0 tracks the analysis exactly, jitter and all.")
                        target: root.selected; propertyName: "smoothMs"
                        from: 0; to: 400; stepSize: 5; unit: "ms"
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.smoothingMode !== "symmetric"
                        label: qsTr("Attack")
                        info: qsTr("How quickly a bar climbs to a new peak. Short is punchy.")
                        target: root.selected; propertyName: "attackMs"
                        from: 0; to: 300; stepSize: 1; unit: "ms"
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.smoothingMode !== "symmetric"
                        label: qsTr("Decay")
                        info: qsTr("How slowly a bar drains back from a peak once the sound "
                                   + "has gone.")
                        target: root.selected; propertyName: "decayMs"
                        from: 10; to: 1500; stepSize: 5; unit: "ms"
                    }
                    ParamSlider {
                        label: qsTr("Neighbour smoothing")
                        info: qsTr("Blends each band toward the ones beside it, turning a comb "
                                   + "of spikes into a single contour.")
                        details: qsTr("This smooths across the frequency axis rather than "
                                      + "across time, so it costs no responsiveness at all — a "
                                      + "hit still lands on the frame it arrives.\n\n"
                                      + "What it removes is the unrelatedness of adjacent "
                                      + "bands. A high band count with Peak measurement gives a "
                                      + "ragged comb where each bar is doing its own thing; "
                                      + "blending neighbours together turns that into a shape "
                                      + "that moves as one. Push it far and the spectrum "
                                      + "becomes a smooth hill with no detail left in it.")
                        target: root.selected
                        propertyName: "spatialSmoothing"
                        from: 0; to: 1
                    }
                    ParamSlider {
                        label: qsTr("Resting level")
                        info: qsTr("The level bands hold when there is no sound at all, so the "
                                   + "display never goes completely flat.")
                        target: root.selected
                        propertyName: "floorLevel"
                        from: 0; to: 0.3; stepSize: 0.001
                    }
                }

                // ------------------------------------------------------- text
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    visible: root.selectedType === "text"

                    SectionHeader { text: qsTr("Content") }

                    AppTextField {
                        Layout.fillWidth: true
                        text: (root.selected && root.selected.text !== undefined)
                              ? root.selected.text : ""
                        placeholder: qsTr("Track title")
                        onTextEdited: if (root.selected) root.selected.text = text
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: qsTr("Use track title")
                        iconPath: Icons.link
                        dense: true
                        enabled: audioEngine.hasTrack && root.selected !== null
                        onClicked: root.selected.text = audioEngine.trackTitle
                    }

                    SectionHeader { text: qsTr("Type") }

                    ParamSlider {
                        label: qsTr("Font size")
                        info: qsTr("Text size as a fraction of canvas height, so it scales "
                                   + "with the export resolution instead of shrinking.")
                        target: root.selected; propertyName: "fontScale"
                        from: 0.01; to: 0.3; stepSize: 0.001
                    }
                    SegmentedControl {
                        label: qsTr("Alignment"); target: root.selected; propertyName: "alignment"
                        info: qsTr("How the text sits within the layer's box.")
                        options: [
                            { text: qsTr("Left"), value: "left" },
                            { text: qsTr("Centre"), value: "center" },
                            { text: qsTr("Right"), value: "right" }
                        ]
                    }
                    ToggleRow {
                        label: qsTr("Bold")
                        info: qsTr("Draws the text in the font's bold weight.")
                        target: root.selected
                        propertyName: "bold"
                    }
                    ColorField {
                        label: qsTr("Colour"); target: root.selected; propertyName: "color"
                        info: qsTr("The colour the text is drawn in.")
                    }
                }

                // -------------------------------------------------- particles
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    visible: root.selectedType === "particles"

                    SectionHeader { text: qsTr("Emission") }

                    SegmentedControl {
                        label: qsTr("Driven by"); target: root.selected; propertyName: "band"
                        info: qsTr("Which part of the spectrum drives emission. This layer "
                                   + "analyses the audio itself, independently of any bars.")
                        options: [
                            { text: qsTr("Bass"), value: "bass" },
                            { text: qsTr("Mid"), value: "mid" },
                            { text: qsTr("Treble"), value: "treble" }
                        ]
                    }
                    ParamSlider {
                        label: qsTr("Idle rate")
                        info: qsTr("Particles emitted per second while the band is silent.")
                        target: root.selected; propertyName: "baseRate"
                        from: 0; to: 400; stepSize: 5; unit: "/s"
                    }
                    ParamSlider {
                        label: qsTr("Reactive rate")
                        info: qsTr("Extra particles per second at full band energy, added on "
                                   + "top of the idle rate.")
                        target: root.selected
                        propertyName: "reactiveRate"
                        from: 0; to: 1500; stepSize: 10; unit: "/s"
                    }
                    ParamSlider {
                        label: qsTr("Lifetime")
                        info: qsTr("How long each particle lasts before it fades out. Long "
                                   + "lifetimes with high rates mean a lot of particles at once.")
                        target: root.selected; propertyName: "lifeSpan"
                        from: 200; to: 6000; stepSize: 50; unit: "ms"
                    }

                    SectionHeader { text: qsTr("Motion") }

                    ParamSlider {
                        label: qsTr("Speed")
                        info: qsTr("How fast particles travel when emitted, in pixels per "
                                   + "second.")
                        target: root.selected; propertyName: "speed"
                        from: 0; to: 900; stepSize: 10
                    }
                    ParamSlider {
                        label: qsTr("Direction")
                        info: qsTr("Which way particles are emitted. 270° is straight up, 90° "
                                   + "straight down.")
                        target: root.selected; propertyName: "angle"
                        from: 0; to: 360; stepSize: 1; unit: "°"
                    }
                    ParamSlider {
                        label: qsTr("Spread")
                        info: qsTr("How far to either side of that direction particles may "
                                   + "scatter. 0 is a single beam.")
                        target: root.selected; propertyName: "angleSpread"
                        from: 0; to: 180; stepSize: 1; unit: "°"
                    }
                    ParamSlider {
                        label: qsTr("Gravity")
                        info: qsTr("Constant downward pull on every particle. Negative makes "
                                   + "them float upward instead.")
                        target: root.selected; propertyName: "gravity"
                        from: -500; to: 900; stepSize: 10
                    }

                    SectionHeader { text: qsTr("Appearance") }

                    ParamSlider {
                        label: qsTr("Size")
                        info: qsTr("How large each particle starts. They shrink away to "
                                   + "nothing over their lifetime.")
                        target: root.selected; propertyName: "particleSize"
                        from: 1; to: 80; stepSize: 1; unit: "px"
                    }
                    ColorField {
                        label: qsTr("Colour"); target: root.selected; propertyName: "color"
                        info: qsTr("The particle colour, varied slightly from one particle to "
                                   + "the next.")
                    }
                }

                // ------------------------------------------------- background
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    visible: root.selectedType === "background"

                    SectionHeader { text: qsTr("Fill") }

                    ColorField {
                        label: qsTr("Colour"); target: root.selected; propertyName: "color"
                        info: qsTr("The flat colour behind everything else. An image, if one "
                                   + "is set, covers it.")
                    }

                    SectionHeader { text: qsTr("Image") }

                    Text {
                        Layout.fillWidth: true
                        visible: root.selected && root.selected.imageSource !== undefined
                                 && root.selected.imageSource !== ""
                        text: root.selected && root.selected.imageSource
                              ? decodeURIComponent(String(root.selected.imageSource)
                                    .split("/").pop())
                              : ""
                        color: Theme.textGhost
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeKicker
                        elide: Text.ElideMiddle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppButton {
                            Layout.fillWidth: true
                            dense: true
                            iconPath: Icons.image
                            text: root.selected && root.selected.imageSource
                                  ? qsTr("Change…") : qsTr("Choose image…")
                            onClicked: imageDialog.open()
                        }

                        IconButton {
                            iconPath: Icons.close
                            iconSize: 13
                            visible: root.selected && root.selected.imageSource !== undefined
                                     && root.selected.imageSource !== ""
                            tip: qsTr("Clear image")
                            onClicked: if (root.selected) root.selected.imageSource = ""
                        }
                    }

                    SegmentedControl {
                        label: qsTr("Fill mode"); target: root.selected; propertyName: "fillMode"
                        info: qsTr("How the image meets the canvas: Crop fills it and loses "
                                   + "the overhang, Fit shows all of it and leaves bars, "
                                   + "Stretch distorts it to fit exactly.")
                        options: [
                            { text: qsTr("Crop"), value: "crop" },
                            { text: qsTr("Fit"), value: "fit" },
                            { text: qsTr("Stretch"), value: "stretch" }
                        ]
                    }
                }
            }
        }
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Choose a background image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: if (root.selected) root.selected.imageSource = selectedFile
    }
}
