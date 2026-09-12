// Emission, motion and appearance for a particle layer.
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
    visible: root.nodeType === "particles"

    SectionHeader { text: qsTr("Emission") }

    SegmentedControl {
        label: qsTr("Driven by"); target: root.node; propertyName: "band"
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
        target: root.node; propertyName: "baseRate"
        from: 0; to: 400; stepSize: 5; unit: "/s"
    }
    ParamSlider {
        label: qsTr("Reactive rate")
        info: qsTr("Extra particles per second at full band energy, added on "
                   + "top of the idle rate.")
        target: root.node
        propertyName: "reactiveRate"
        from: 0; to: 1500; stepSize: 10; unit: "/s"
    }
    ParamSlider {
        label: qsTr("Lifetime")
        info: qsTr("How long each particle lasts before it fades out. Long "
                   + "lifetimes with high rates mean a lot of particles at once.")
        target: root.node; propertyName: "lifeSpan"
        from: 200; to: 6000; stepSize: 50; unit: "ms"
    }

    SectionHeader { text: qsTr("Motion") }

    ParamSlider {
        label: qsTr("Speed")
        info: qsTr("How fast particles travel when emitted, in pixels per "
                   + "second.")
        target: root.node; propertyName: "speed"
        from: 0; to: 900; stepSize: 10
    }
    ParamSlider {
        label: qsTr("Direction")
        info: qsTr("Which way particles are emitted. 270° is straight up, 90° "
                   + "straight down.")
        target: root.node; propertyName: "angle"
        from: 0; to: 360; stepSize: 1; unit: "°"
    }
    ParamSlider {
        label: qsTr("Spread")
        info: qsTr("How far to either side of that direction particles may "
                   + "scatter. 0 is a single beam.")
        target: root.node; propertyName: "angleSpread"
        from: 0; to: 180; stepSize: 1; unit: "°"
    }
    ParamSlider {
        label: qsTr("Gravity")
        info: qsTr("Constant downward pull on every particle. Negative makes "
                   + "them float upward instead.")
        target: root.node; propertyName: "gravity"
        from: -500; to: 900; stepSize: 10
    }

    SectionHeader { text: qsTr("Appearance") }

    ParamSlider {
        label: qsTr("Size")
        info: qsTr("How large each particle starts. They shrink away to "
                   + "nothing over their lifetime.")
        target: root.node; propertyName: "particleSize"
        from: 1; to: 80; stepSize: 1; unit: "px"
    }
    ColorField {
        label: qsTr("Colour"); target: root.node; propertyName: "color"
        info: qsTr("The particle colour, varied slightly from one particle to "
                   + "the next.")
    }
}
