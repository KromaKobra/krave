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

import MusicVisualizer
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

                    Text {
                        text: qsTr("Name")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        text: root.selected ? root.selected.name : ""
                        onTextEdited: if (root.selected) root.selected.name = text
                    }
                }

                ToggleRow {
                    label: qsTr("Visible")
                    target: root.selected
                    propertyName: "layerVisible"
                }

                ParamSlider {
                    label: qsTr("Opacity"); target: root.selected; propertyName: "layerOpacity"
                    from: 0; to: 1
                }

                SegmentedControl {
                    label: qsTr("Blend mode"); target: root.selected; propertyName: "blendMode"
                    options: [
                        { text: qsTr("Normal"), value: "normal" },
                        { text: qsTr("Add"), value: "add" },
                        { text: qsTr("Screen"), value: "screen" }
                    ]
                }

                // -------------------------------------------------- placement
                SectionHeader { text: qsTr("Placement") }

                ParamSlider {
                    label: qsTr("Centre X"); target: root.selected; propertyName: "posX"
                    from: -0.5; to: 1.5
                }
                ParamSlider {
                    label: qsTr("Centre Y"); target: root.selected; propertyName: "posY"
                    from: -0.5; to: 1.5
                }
                ParamSlider {
                    label: qsTr("Width"); target: root.selected; propertyName: "sizeW"
                    from: 0; to: 2
                }
                ParamSlider {
                    label: qsTr("Height"); target: root.selected; propertyName: "sizeH"
                    from: 0; to: 2
                }
                ParamSlider {
                    label: qsTr("Rotation"); target: root.selected; propertyName: "layerRotation"
                    from: -180; to: 180; stepSize: 1; decimals: 0; unit: "°"
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
                              ? qsTr("%1 bars — the audio layer's bands, mirrored")
                                  .arg(root.selected.barCount)
                              : qsTr("%1 bars, set by the audio layer's band count")
                                  .arg(root.selected ? root.selected.barCount : 0)
                    }

                    SegmentedControl {
                        label: qsTr("Orientation")
                        target: root.selected; propertyName: "orientation"
                        options: [
                            { text: qsTr("Linear"), value: "linear" },
                            { text: qsTr("Mirrored"), value: "mirrored" },
                            { text: qsTr("Radial"), value: "radial" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.orientation === "radial"
                        label: qsTr("Inner radius"); target: root.selected
                        propertyName: "innerRadius"
                        from: 0.05; to: 0.9
                    }
                    ParamSlider {
                        label: qsTr("Bar width"); target: root.selected
                        propertyName: "barWidthRatio"
                        from: 0.05; to: 1
                    }
                    ParamSlider {
                        label: qsTr("Corner radius"); target: root.selected
                        propertyName: "cornerRadius"
                        from: 0; to: 24; stepSize: 0.5; decimals: 1; unit: "px"
                    }
                    ParamSlider {
                        label: qsTr("Min height"); target: root.selected; propertyName: "minHeight"
                        from: 0; to: 0.5; decimals: 3
                    }
                    ParamSlider {
                        label: qsTr("Max height"); target: root.selected; propertyName: "maxHeight"
                        from: 0.05; to: 1
                    }

                    ToggleRow {
                        label: qsTr("Mirror spectrum")
                        description: qsTr("Bass at the outer edges")
                        target: root.selected
                        propertyName: "mirrorSpectrum"
                    }

                    SectionHeader { text: qsTr("Colour") }

                    SegmentedControl {
                        label: qsTr("Mode"); target: root.selected; propertyName: "colorMode"
                        options: [
                            { text: qsTr("Solid"), value: "solid" },
                            { text: qsTr("Gradient"), value: "gradient" },
                            { text: qsTr("Spectrum"), value: "spectrum" }
                        ]
                    }
                    ColorField {
                        label: qsTr("Colour A"); target: root.selected; propertyName: "colorA"
                        visible: root.selected && root.selected.colorMode !== "spectrum"
                    }
                    ColorField {
                        label: qsTr("Colour B"); target: root.selected; propertyName: "colorB"
                        visible: root.selected && root.selected.colorMode === "gradient"
                    }
                    ParamSlider {
                        label: qsTr("Reactive brightness"); target: root.selected
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
                        label: qsTr("Band count"); target: root.selected; propertyName: "bandCount"
                        from: 4; to: 256; stepSize: 1; decimals: 0
                    }
                    ParamSlider {
                        label: qsTr("Window"); target: root.selected; propertyName: "windowMs"
                        from: 10; to: 400; stepSize: 5; decimals: 0; unit: "ms"
                    }
                    ParamSlider {
                        label: qsTr("Windows averaged"); target: root.selected
                        propertyName: "averageCount"
                        from: 1; to: 16; stepSize: 1; decimals: 0
                    }

                    SectionHeader { text: qsTr("Frequency") }

                    ParamSlider {
                        label: qsTr("Low cut"); target: root.selected; propertyName: "minHz"
                        from: 10; to: 500; stepSize: 5; decimals: 0; unit: "Hz"
                    }
                    ParamSlider {
                        label: qsTr("High cut"); target: root.selected; propertyName: "maxHz"
                        from: 200; to: 22000; stepSize: 100; decimals: 0; unit: "Hz"
                    }
                    ParamSlider {
                        label: qsTr("Linear ←→ log spacing"); target: root.selected
                        propertyName: "hzLinearFactor"
                        from: 0; to: 1
                    }

                    SectionHeader { text: qsTr("Response") }

                    SegmentedControl {
                        label: qsTr("Amplitude"); target: root.selected
                        propertyName: "amplitudeMode"
                        options: [
                            { text: qsTr("Decibel"), value: "db" },
                            { text: qsTr("Linear"), value: "linear" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.amplitudeMode === "linear"
                        label: qsTr("Gain"); target: root.selected; propertyName: "linearGain"
                        from: 0.5; to: 60; stepSize: 0.5; decimals: 1
                    }
                    ParamSlider {
                        label: qsTr("Low-frequency level"); target: root.selected
                        propertyName: "lowFreqMagnitude"
                        from: 0; to: 1
                    }
                    SegmentedControl {
                        label: qsTr("Band measure"); target: root.selected
                        propertyName: "bandMeasure"
                        options: [
                            { text: qsTr("Average"), value: "rms" },
                            { text: qsTr("Peak"), value: "peak" }
                        ]
                    }
                    ParamSlider {
                        label: qsTr("Treble lift"); target: root.selected
                        propertyName: "tiltDbPerOctave"
                        from: 0; to: 9; stepSize: 0.25; decimals: 2; unit: "dB"
                    }
                    ParamSlider {
                        label: qsTr("Per-band auto-gain"); target: root.selected
                        propertyName: "bandNormalization"
                        from: 0; to: 1
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.bandNormalization > 0
                        label: qsTr("Auto-gain adapt"); target: root.selected
                        propertyName: "normalizationTau"
                        from: 0.2; to: 10; stepSize: 0.1; decimals: 1; unit: "s"
                    }
                    ParamSlider {
                        label: qsTr("Floor"); target: root.selected; propertyName: "floorDb"
                        from: -100; to: -20; stepSize: 1; decimals: 0; unit: "dB"
                    }
                    ParamSlider {
                        label: qsTr("Ceiling"); target: root.selected; propertyName: "ceilingDb"
                        from: -40; to: 6; stepSize: 1; decimals: 0; unit: "dB"
                    }
                    ParamSlider {
                        label: qsTr("Gate"); target: root.selected; propertyName: "gateDb"
                        from: 0; to: 24; stepSize: 1; decimals: 0; unit: "dB"
                    }
                    SegmentedControl {
                        label: qsTr("Scaling curve"); target: root.selected
                        propertyName: "scalingCurve"
                        options: [
                            { text: qsTr("Power"), value: "power" },
                            { text: qsTr("Linear"), value: "linear" },
                            { text: qsTr("Log"), value: "log" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.scalingCurve === "power"
                        label: qsTr("Gamma"); target: root.selected; propertyName: "gamma"
                        from: 0.2; to: 4
                    }

                    SectionHeader { text: qsTr("Motion") }

                    EnumCombo {
                        label: qsTr("Smoothing"); target: root.selected
                        propertyName: "smoothingMode"
                        options: [
                            { text: qsTr("Symmetric (natural)"), value: "symmetric" },
                            { text: qsTr("Attack / decay (punchy)"), value: "attackDecay" }
                        ]
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.smoothingMode === "symmetric"
                        label: qsTr("Smoothing"); target: root.selected; propertyName: "smoothMs"
                        from: 0; to: 400; stepSize: 5; decimals: 0; unit: "ms"
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.smoothingMode !== "symmetric"
                        label: qsTr("Attack"); target: root.selected; propertyName: "attackMs"
                        from: 0; to: 300; stepSize: 1; decimals: 0; unit: "ms"
                    }
                    ParamSlider {
                        visible: root.selected && root.selected.smoothingMode !== "symmetric"
                        label: qsTr("Decay"); target: root.selected; propertyName: "decayMs"
                        from: 10; to: 1500; stepSize: 5; decimals: 0; unit: "ms"
                    }
                    ParamSlider {
                        label: qsTr("Neighbour smoothing"); target: root.selected
                        propertyName: "spatialSmoothing"
                        from: 0; to: 1
                    }
                    ParamSlider {
                        label: qsTr("Resting level"); target: root.selected
                        propertyName: "floorLevel"
                        from: 0; to: 0.3; decimals: 3
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
                        label: qsTr("Font size"); target: root.selected; propertyName: "fontScale"
                        from: 0.01; to: 0.3; decimals: 3
                    }
                    SegmentedControl {
                        label: qsTr("Alignment"); target: root.selected; propertyName: "alignment"
                        options: [
                            { text: qsTr("Left"), value: "left" },
                            { text: qsTr("Centre"), value: "center" },
                            { text: qsTr("Right"), value: "right" }
                        ]
                    }
                    ToggleRow {
                        label: qsTr("Bold")
                        target: root.selected
                        propertyName: "bold"
                    }
                    ColorField {
                        label: qsTr("Colour"); target: root.selected; propertyName: "color"
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
                        options: [
                            { text: qsTr("Bass"), value: "bass" },
                            { text: qsTr("Mid"), value: "mid" },
                            { text: qsTr("Treble"), value: "treble" }
                        ]
                    }
                    ParamSlider {
                        label: qsTr("Idle rate"); target: root.selected; propertyName: "baseRate"
                        from: 0; to: 400; stepSize: 5; decimals: 0; unit: "/s"
                    }
                    ParamSlider {
                        label: qsTr("Reactive rate"); target: root.selected
                        propertyName: "reactiveRate"
                        from: 0; to: 1500; stepSize: 10; decimals: 0; unit: "/s"
                    }
                    ParamSlider {
                        label: qsTr("Lifetime"); target: root.selected; propertyName: "lifeSpan"
                        from: 200; to: 6000; stepSize: 50; decimals: 0; unit: "ms"
                    }

                    SectionHeader { text: qsTr("Motion") }

                    ParamSlider {
                        label: qsTr("Speed"); target: root.selected; propertyName: "speed"
                        from: 0; to: 900; stepSize: 10; decimals: 0
                    }
                    ParamSlider {
                        label: qsTr("Direction"); target: root.selected; propertyName: "angle"
                        from: 0; to: 360; stepSize: 1; decimals: 0; unit: "°"
                    }
                    ParamSlider {
                        label: qsTr("Spread"); target: root.selected; propertyName: "angleSpread"
                        from: 0; to: 180; stepSize: 1; decimals: 0; unit: "°"
                    }
                    ParamSlider {
                        label: qsTr("Gravity"); target: root.selected; propertyName: "gravity"
                        from: -500; to: 900; stepSize: 10; decimals: 0
                    }

                    SectionHeader { text: qsTr("Appearance") }

                    ParamSlider {
                        label: qsTr("Size"); target: root.selected; propertyName: "particleSize"
                        from: 1; to: 80; stepSize: 1; decimals: 0; unit: "px"
                    }
                    ColorField {
                        label: qsTr("Colour"); target: root.selected; propertyName: "color"
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
