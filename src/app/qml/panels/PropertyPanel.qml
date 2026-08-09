// Editor for the selected layer.
//
// Every control writes directly to a Q_PROPERTY on the C++ Layer object. Because
// the canvas binds to those same properties, edits are visible on the next frame
// -- there is no apply step and no reload.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "../controls"

Flickable {
    id: root

    readonly property var selected: layerModel.selectedLayer

    contentWidth: width
    contentHeight: column.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        width: root.width
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: root.selected ? root.selected.name : qsTr("No layer selected")
            color: "#e0e0ea"
            font.pixelSize: 12
            font.bold: true
            elide: Text.ElideRight
        }

        component SectionHeader: Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            color: "#55556a"
            font.pixelSize: 10
            font.bold: true
        }

        // ---------------------------------------------------------- common
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.selected !== null

            SectionHeader { text: qsTr("PLACEMENT") }

            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Center X"); target: root.selected; propertyName: "posX"
                from: -0.5; to: 1.5
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Center Y"); target: root.selected; propertyName: "posY"
                from: -0.5; to: 1.5
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Width"); target: root.selected; propertyName: "sizeW"
                from: 0; to: 2
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Height"); target: root.selected; propertyName: "sizeH"
                from: 0; to: 2
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Rotation"); target: root.selected; propertyName: "layerRotation"
                from: -180; to: 180; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Opacity"); target: root.selected; propertyName: "layerOpacity"
                from: 0; to: 1
            }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Blend mode"); target: root.selected; propertyName: "blendMode"
                options: [
                    { text: qsTr("Normal"), value: "normal" },
                    { text: qsTr("Add"), value: "add" },
                    { text: qsTr("Screen"), value: "screen" }
                ]
            }
        }

        // ------------------------------------------------------------ bars
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.selected !== null && root.selected.typeId === "bars"

            SectionHeader { text: qsTr("BARS") }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#55556a"
                font.pixelSize: 10
                text: root.selected && root.selected.mirrorSpectrum
                      ? qsTr("%1 bars (audio provider bands, mirrored)")
                          .arg(root.selected.barCount)
                      : qsTr("%1 bars, set by the audio provider's band count")
                          .arg(root.selected ? root.selected.barCount : 0)
            }

            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Bar width"); target: root.selected; propertyName: "barWidthRatio"
                from: 0.05; to: 1
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Corner radius"); target: root.selected; propertyName: "cornerRadius"
                from: 0; to: 24; stepSize: 0.5; decimals: 1
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Min height"); target: root.selected; propertyName: "minHeight"
                from: 0; to: 0.5; decimals: 3
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Max height"); target: root.selected; propertyName: "maxHeight"
                from: 0.05; to: 1
            }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Orientation"); target: root.selected; propertyName: "orientation"
                options: [
                    { text: qsTr("Linear"), value: "linear" },
                    { text: qsTr("Mirrored"), value: "mirrored" },
                    { text: qsTr("Radial"), value: "radial" }
                ]
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.orientation === "radial"
                label: qsTr("Inner radius"); target: root.selected; propertyName: "innerRadius"
                from: 0.05; to: 0.9
            }

            CheckBox {
                Layout.fillWidth: true
                // Sections for other layer types stay instantiated while hidden,
                // so this can be pointed at a layer with no such property.
                checked: (root.selected && root.selected.mirrorSpectrum !== undefined)
                         ? root.selected.mirrorSpectrum : false
                onToggled: if (root.selected) root.selected.mirrorSpectrum = checked
                contentItem: Text {
                    leftPadding: 22
                    text: qsTr("Mirror spectrum (bass at the edges)")
                    color: "#c8c8d4"
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                }
            }

            SectionHeader { text: qsTr("COLOUR") }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Mode"); target: root.selected; propertyName: "colorMode"
                options: [
                    { text: qsTr("Solid"), value: "solid" },
                    { text: qsTr("Gradient"), value: "gradient" },
                    { text: qsTr("Spectrum"), value: "spectrum" }
                ]
            }
            ColorField {
                Layout.fillWidth: true
                label: qsTr("Colour A"); target: root.selected; propertyName: "colorA"
                visible: root.selected && root.selected.colorMode !== "spectrum"
            }
            ColorField {
                Layout.fillWidth: true
                label: qsTr("Colour B"); target: root.selected; propertyName: "colorB"
                visible: root.selected && root.selected.colorMode === "gradient"
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Reactive brightness"); target: root.selected
                propertyName: "reactiveBrightness"
                from: 0; to: 2
            }

            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                wrapMode: Text.WordWrap
                color: "#55556a"
                font.pixelSize: 10
                text: qsTr("Frequency response, smoothing and dynamics live on the "
                           + "Audio provider layer.")
            }
        }

        // -------------------------------------------------- audio provider
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.selected !== null && root.selected.typeId === "audio"

            SectionHeader { text: qsTr("RESOLUTION") }

            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Band count"); target: root.selected; propertyName: "bandCount"
                from: 4; to: 256; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Window (ms)"); target: root.selected; propertyName: "windowMs"
                from: 10; to: 400; stepSize: 5; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Windows averaged"); target: root.selected
                propertyName: "averageCount"
                from: 1; to: 16; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Min frequency (Hz)"); target: root.selected; propertyName: "minHz"
                from: 10; to: 500; stepSize: 5; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Max frequency (Hz)"); target: root.selected; propertyName: "maxHz"
                from: 200; to: 22000; stepSize: 100; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Linear ← → log spacing"); target: root.selected
                propertyName: "hzLinearFactor"
                from: 0; to: 1
            }

            SectionHeader { text: qsTr("RESPONSE") }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Amplitude"); target: root.selected; propertyName: "amplitudeMode"
                options: [
                    { text: qsTr("Decibel (even, filled)"), value: "db" },
                    { text: qsTr("Linear (flat floor, peaks)"), value: "linear" }
                ]
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.amplitudeMode === "linear"
                label: qsTr("Gain"); target: root.selected; propertyName: "linearGain"
                from: 0.5; to: 60; stepSize: 0.5; decimals: 1
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Low-frequency level"); target: root.selected
                propertyName: "lowFreqMagnitude"
                from: 0; to: 1
            }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Band measure"); target: root.selected; propertyName: "bandMeasure"
                options: [
                    { text: qsTr("Average (smooth)"), value: "rms" },
                    { text: qsTr("Peak (jumpy)"), value: "peak" }
                ]
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Treble lift (dB/octave)"); target: root.selected
                propertyName: "tiltDbPerOctave"
                from: 0; to: 9; stepSize: 0.25; decimals: 2
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Per-band auto-gain"); target: root.selected
                propertyName: "bandNormalization"
                from: 0; to: 1
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.bandNormalization > 0
                label: qsTr("Auto-gain adapt (s)"); target: root.selected
                propertyName: "normalizationTau"
                from: 0.2; to: 10; stepSize: 0.1; decimals: 1
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Floor (dB)"); target: root.selected; propertyName: "floorDb"
                from: -100; to: -20; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Ceiling (dB)"); target: root.selected; propertyName: "ceilingDb"
                from: -40; to: 6; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Gate (dB)"); target: root.selected; propertyName: "gateDb"
                from: 0; to: 24; stepSize: 1; decimals: 0
            }
            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Scaling curve"); target: root.selected; propertyName: "scalingCurve"
                options: [
                    { text: qsTr("Power"), value: "power" },
                    { text: qsTr("Linear"), value: "linear" },
                    { text: qsTr("Logarithmic"), value: "log" }
                ]
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.scalingCurve === "power"
                label: qsTr("Gamma"); target: root.selected; propertyName: "gamma"
                from: 0.2; to: 4
            }

            SectionHeader { text: qsTr("MOTION") }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Smoothing"); target: root.selected; propertyName: "smoothingMode"
                options: [
                    { text: qsTr("Symmetric (natural)"), value: "symmetric" },
                    { text: qsTr("Attack / decay (punchy)"), value: "attackDecay" }
                ]
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.smoothingMode === "symmetric"
                label: qsTr("Smoothing (ms)"); target: root.selected; propertyName: "smoothMs"
                from: 0; to: 400; stepSize: 5; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.smoothingMode !== "symmetric"
                label: qsTr("Attack (ms)"); target: root.selected; propertyName: "attackMs"
                from: 0; to: 300; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                visible: root.selected && root.selected.smoothingMode !== "symmetric"
                label: qsTr("Decay (ms)"); target: root.selected; propertyName: "decayMs"
                from: 10; to: 1500; stepSize: 5; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Neighbour smoothing"); target: root.selected
                propertyName: "spatialSmoothing"
                from: 0; to: 1
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Resting level"); target: root.selected; propertyName: "floorLevel"
                from: 0; to: 0.3; decimals: 3
            }
        }

        // ------------------------------------------------------------ text
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.selected !== null && root.selected.typeId === "text"

            SectionHeader { text: qsTr("TEXT") }

            TextField {
                Layout.fillWidth: true
                text: (root.selected && root.selected.text !== undefined)
                      ? root.selected.text : ""
                color: "#e0e0ea"
                font.pixelSize: 11
                onTextEdited: if (root.selected) root.selected.text = text
                background: Rectangle {
                    color: "#1c1c26"
                    border.color: parent.activeFocus ? "#4cc9f0" : "#2f2f3d"
                    border.width: 1
                    radius: 3
                }
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Use track title")
                enabled: audioEngine.hasTrack && root.selected
                onClicked: root.selected.text = audioEngine.trackTitle
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#c8c8d4" : "#4a4a58"
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    radius: 3
                    color: parent.down ? "#2f2f3d" : "#1c1c26"
                    border.color: "#2f2f3d"
                    border.width: 1
                }
            }

            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Font size"); target: root.selected; propertyName: "fontScale"
                from: 0.01; to: 0.3; decimals: 3
            }
            ColorField {
                Layout.fillWidth: true
                label: qsTr("Colour"); target: root.selected; propertyName: "color"
            }
            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Alignment"); target: root.selected; propertyName: "alignment"
                options: [
                    { text: qsTr("Left"), value: "left" },
                    { text: qsTr("Center"), value: "center" },
                    { text: qsTr("Right"), value: "right" }
                ]
            }
        }

        // ------------------------------------------------------- particles
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.selected !== null && root.selected.typeId === "particles"

            SectionHeader { text: qsTr("PARTICLES") }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Driven by"); target: root.selected; propertyName: "band"
                options: [
                    { text: qsTr("Bass"), value: "bass" },
                    { text: qsTr("Mid"), value: "mid" },
                    { text: qsTr("Treble"), value: "treble" }
                ]
            }
            ColorField {
                Layout.fillWidth: true
                label: qsTr("Colour"); target: root.selected; propertyName: "color"
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Idle rate (/s)"); target: root.selected; propertyName: "baseRate"
                from: 0; to: 400; stepSize: 5; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Reactive rate (/s)"); target: root.selected
                propertyName: "reactiveRate"
                from: 0; to: 1500; stepSize: 10; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Size"); target: root.selected; propertyName: "particleSize"
                from: 1; to: 80; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Lifetime (ms)"); target: root.selected; propertyName: "lifeSpan"
                from: 200; to: 6000; stepSize: 50; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Speed"); target: root.selected; propertyName: "speed"
                from: 0; to: 900; stepSize: 10; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Direction (°)"); target: root.selected; propertyName: "angle"
                from: 0; to: 360; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Spread (°)"); target: root.selected; propertyName: "angleSpread"
                from: 0; to: 180; stepSize: 1; decimals: 0
            }
            LabeledSlider {
                Layout.fillWidth: true
                label: qsTr("Gravity"); target: root.selected; propertyName: "gravity"
                from: -500; to: 900; stepSize: 10; decimals: 0
            }
        }

        // ------------------------------------------------------ background
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.selected !== null && root.selected.typeId === "background"

            SectionHeader { text: qsTr("BACKGROUND") }

            ColorField {
                Layout.fillWidth: true
                label: qsTr("Colour"); target: root.selected; propertyName: "color"
            }

            Button {
                Layout.fillWidth: true
                text: root.selected && root.selected.imageSource
                      ? qsTr("Change image…") : qsTr("Choose image…")
                onClicked: imageDialog.open()
                contentItem: Text {
                    text: parent.text
                    color: "#c8c8d4"
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    radius: 3
                    color: parent.down ? "#2f2f3d" : "#1c1c26"
                    border.color: "#2f2f3d"
                    border.width: 1
                }
            }

            Button {
                Layout.fillWidth: true
                visible: root.selected && root.selected.imageSource !== undefined
                         && root.selected.imageSource !== ""
                text: qsTr("Clear image")
                onClicked: if (root.selected) root.selected.imageSource = ""
                contentItem: Text {
                    text: parent.text
                    color: "#c8c8d4"
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    radius: 3
                    color: parent.down ? "#2f2f3d" : "#1c1c26"
                    border.color: "#2f2f3d"
                    border.width: 1
                }
            }

            EnumCombo {
                Layout.fillWidth: true
                label: qsTr("Fill mode"); target: root.selected; propertyName: "fillMode"
                options: [
                    { text: qsTr("Crop"), value: "crop" },
                    { text: qsTr("Fit"), value: "fit" },
                    { text: qsTr("Stretch"), value: "stretch" }
                ]
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 12 }
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Choose a background image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: if (root.selected) root.selected.imageSource = selectedFile
    }
}
