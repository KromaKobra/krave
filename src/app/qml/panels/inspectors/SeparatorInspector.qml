// Analysis settings for a separator node.
//
// The largest of the per-type blocks by a wide margin: every knob that turns a
// stream of samples into the numbers the bars draw is here, because that is the
// whole purpose of the node. See Node.h for what each one does to the DSP -- and
// for why a separator reads the PCM at the root of its audio chain rather than its
// immediate input's bands.
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
    visible: root.nodeType === "separator"

    SectionHeader { text: qsTr("Resolution") }

    ParamSlider {
        label: qsTr("Band count")
        info: qsTr("How many bands the spectrum is split into. Every spectrum node "
                   + "wired to this one draws this many bars.")
        target: root.node; propertyName: "bandCount"
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
        target: root.node; propertyName: "windowMs"
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
        target: root.node
        propertyName: "averageCount"
        from: 1; to: 16; stepSize: 1
    }

    SectionHeader { text: qsTr("Frequency") }

    ParamSlider {
        label: qsTr("Low cut")
        info: qsTr("The lowest frequency given a band. Everything below it is "
                   + "left out of the display.")
        target: root.node; propertyName: "minHz"
        from: 10; to: 500; stepSize: 5; unit: "Hz"
    }
    ParamSlider {
        label: qsTr("High cut")
        info: qsTr("The highest frequency given a band. Most music has little "
                   + "left above 16 kHz.")
        target: root.node; propertyName: "maxHz"
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
        target: root.node
        propertyName: "hzLinearFactor"
        from: 0; to: 1
    }

    SectionHeader { text: qsTr("Response") }

    SegmentedControl {
        label: qsTr("Amplitude"); target: root.node
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
        visible: root.node && root.node.amplitudeMode === "linear"
        label: qsTr("Gain")
        info: qsTr("Linear mode only: multiplies the raw magnitude before it "
                   + "is drawn.")
        target: root.node; propertyName: "linearGain"
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
        target: root.node
        propertyName: "lowFreqMagnitude"
        from: 0; to: 1
    }
    SegmentedControl {
        label: qsTr("Band measure"); target: root.node
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
        target: root.node
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
        target: root.node
        propertyName: "bandNormalization"
        from: 0; to: 1
    }
    ParamSlider {
        visible: root.node && root.node.bandNormalization > 0
        label: qsTr("Auto-gain adapt")
        info: qsTr("How long the auto-gain takes to follow a change in level. "
                   + "Longer is steadier and less prone to pumping.")
        target: root.node
        propertyName: "normalizationTau"
        from: 0.2; to: 10; stepSize: 0.1; unit: "s"
    }
    ParamSlider {
        label: qsTr("Floor")
        info: qsTr("The level treated as silence. Anything at or below it sits "
                   + "at the bottom of the bar.")
        target: root.node; propertyName: "floorDb"
        from: -100; to: -20; stepSize: 1; unit: "dB"
    }
    ParamSlider {
        label: qsTr("Ceiling")
        info: qsTr("The level treated as full scale. Anything at or above it "
                   + "reaches full height.")
        target: root.node; propertyName: "ceilingDb"
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
        target: root.node; propertyName: "gateDb"
        from: 0; to: 24; stepSize: 1; unit: "dB"
    }
    SegmentedControl {
        label: qsTr("Scaling curve"); target: root.node
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
        visible: root.node && root.node.scalingCurve === "power"
        label: qsTr("Gamma")
        info: qsTr("The power curve's exponent: below 1 lifts quiet levels, "
                   + "above 1 pushes them down.")
        target: root.node; propertyName: "gamma"
        from: 0.2; to: 4
    }

    SectionHeader { text: qsTr("Motion") }

    EnumCombo {
        label: qsTr("Smoothing"); target: root.node
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
        visible: root.node && root.node.smoothingMode === "symmetric"
        label: qsTr("Smoothing")
        info: qsTr("How long a bar takes to follow a change, in both "
                   + "directions. 0 tracks the analysis exactly, jitter and all.")
        target: root.node; propertyName: "smoothMs"
        from: 0; to: 400; stepSize: 5; unit: "ms"
    }
    ParamSlider {
        visible: root.node && root.node.smoothingMode !== "symmetric"
        label: qsTr("Attack")
        info: qsTr("How quickly a bar climbs to a new peak. Short is punchy.")
        target: root.node; propertyName: "attackMs"
        from: 0; to: 300; stepSize: 1; unit: "ms"
    }
    ParamSlider {
        visible: root.node && root.node.smoothingMode !== "symmetric"
        label: qsTr("Decay")
        info: qsTr("How slowly a bar drains back from a peak once the sound "
                   + "has gone.")
        target: root.node; propertyName: "decayMs"
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
        target: root.node
        propertyName: "spatialSmoothing"
        from: 0; to: 1
    }
    ParamSlider {
        label: qsTr("Resting level")
        info: qsTr("The level bands hold when there is no sound at all, so the "
                   + "display never goes completely flat.")
        target: root.node
        propertyName: "floorLevel"
        from: 0; to: 0.3; stepSize: 0.001
    }
}
