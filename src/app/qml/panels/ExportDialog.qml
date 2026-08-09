import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: root

    anchors.centerIn: Overlay.overlay
    modal: true
    closePolicy: exportController.exporting
                 ? Popup.NoAutoClose
                 : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
    title: qsTr("Export video")
    width: 480

    readonly property var resolutionPresets: [
        { text: "1280 × 720",   w: 1280, h: 720 },
        { text: "1920 × 1080",  w: 1920, h: 1080 },
        { text: "2560 × 1440",  w: 2560, h: 1440 },
        { text: "3840 × 2160",  w: 3840, h: 2160 },
        { text: "1080 × 1080 (square)", w: 1080, h: 1080 },
        { text: "1080 × 1920 (vertical)", w: 1080, h: 1920 }
    ]

    function formatEta(seconds) {
        if (!(seconds >= 0))
            return qsTr("estimating…")
        const total = Math.round(seconds)
        if (total < 60)
            return qsTr("%1s remaining").arg(total)
        return qsTr("%1m %2s remaining").arg(Math.floor(total / 60)).arg(total % 60)
    }

    background: Rectangle {
        color: "#16161f"
        border.color: "#33334a"
        border.width: 1
        radius: 4
    }

    header: Text {
        text: root.title
        color: "#e0e0ea"
        font.pixelSize: 13
        font.bold: true
        padding: 14
    }

    component FieldLabel: Text {
        color: "#8a8a9a"
        font.pixelSize: 11
    }

    component Field: Rectangle {
        implicitHeight: 26
        color: "#1c1c26"
        border.color: "#2f2f3d"
        border.width: 1
        radius: 3
    }

    contentItem: ColumnLayout {
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8
            enabled: !exportController.exporting

            FieldLabel { text: qsTr("Preset") }
            ComboBox {
                id: presetCombo
                Layout.fillWidth: true
                implicitHeight: 28
                model: root.resolutionPresets
                textRole: "text"
                currentIndex: 1
                onActivated: (i) => {
                    exportController.width = root.resolutionPresets[i].w
                    exportController.height = root.resolutionPresets[i].h
                }
                contentItem: Text {
                    leftPadding: 8
                    text: presetCombo.displayText
                    color: "#e0e0ea"
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                }
                background: Field {}
            }

            FieldLabel { text: qsTr("Resolution") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                SpinBox {
                    Layout.fillWidth: true
                    from: 16; to: 7680; stepSize: 2
                    value: exportController.width
                    editable: true
                    onValueModified: exportController.width = value
                    implicitHeight: 28
                }
                FieldLabel { text: "×" }
                SpinBox {
                    Layout.fillWidth: true
                    from: 16; to: 4320; stepSize: 2
                    value: exportController.height
                    editable: true
                    onValueModified: exportController.height = value
                    implicitHeight: 28
                }
            }

            FieldLabel { text: qsTr("Frame rate") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: [24, 30, 60]
                    Button {
                        required property int modelData
                        text: modelData + " fps"
                        implicitHeight: 26
                        onClicked: exportController.fps = modelData
                        contentItem: Text {
                            text: parent.text
                            color: exportController.fps === parent.modelData
                                   ? "#0b0b12" : "#c8c8d4"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 3
                            color: exportController.fps === parent.modelData
                                   ? "#4cc9f0" : "#1c1c26"
                            border.color: "#2f2f3d"
                            border.width: 1
                        }
                    }
                }

                SpinBox {
                    Layout.fillWidth: true
                    from: 1; to: 240
                    value: exportController.fps
                    editable: true
                    onValueModified: exportController.fps = value
                    implicitHeight: 26
                }
            }

            FieldLabel { text: qsTr("Format") }
            ComboBox {
                id: codecCombo
                Layout.fillWidth: true
                implicitHeight: 28
                model: [
                    { text: qsTr("MP4 — H.264 (widely compatible)"), value: "h264" },
                    { text: qsTr("MOV — ProRes 422 HQ (editing)"), value: "prores" }
                ]
                textRole: "text"
                valueRole: "value"
                currentIndex: exportController.videoCodec === "prores" ? 1 : 0
                onActivated: (i) => exportController.videoCodec = model[i].value
                contentItem: Text {
                    leftPadding: 8
                    rightPadding: 24
                    text: codecCombo.displayText
                    color: "#e0e0ea"
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                background: Field {}
            }

            FieldLabel {
                text: qsTr("Quality")
                visible: exportController.videoCodec === "h264"
            }
            RowLayout {
                Layout.fillWidth: true
                visible: exportController.videoCodec === "h264"
                spacing: 8

                Slider {
                    Layout.fillWidth: true
                    from: 12; to: 30; stepSize: 1
                    value: exportController.quality
                    onMoved: exportController.quality = value
                }
                Text {
                    text: qsTr("CRF %1").arg(exportController.quality)
                    color: "#8a8a9a"
                    font.pixelSize: 11
                    font.family: "Consolas"
                }
            }

            FieldLabel { text: qsTr("Audio") }
            CheckBox {
                Layout.fillWidth: true
                checked: exportController.includeAudio
                onToggled: exportController.includeAudio = checked
                contentItem: Text {
                    leftPadding: 22
                    text: qsTr("Embed the original audio track")
                    color: "#c8c8d4"
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#2a2a38"
        }

        // ------------------------------------------------------------ summary
        Text {
            Layout.fillWidth: true
            visible: !exportController.exporting
            wrapMode: Text.WordWrap
            color: "#6a6a80"
            font.pixelSize: 11
            text: {
                if (!audioEngine.hasTrack)
                    return qsTr("Import an audio track first.")
                const frames = Math.ceil(audioEngine.duration * exportController.fps)
                return qsTr("%1 frames at %2 fps — %3 of audio")
                    .arg(frames)
                    .arg(exportController.fps)
                    .arg(audioEngine.duration.toFixed(1) + "s")
            }
        }

        // ----------------------------------------------------------- progress
        ColumnLayout {
            Layout.fillWidth: true
            visible: exportController.exporting
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: "#1c1c26"

                Rectangle {
                    width: parent.width * exportController.progress
                    height: parent.height
                    radius: 3
                    color: "#4cc9f0"
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("Frame %1 of %2")
                        .arg(exportController.framesDone)
                        .arg(exportController.totalFrames)
                    color: "#c8c8d4"
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: root.formatEta(exportController.etaSeconds)
                    color: "#6a6a80"
                    font.pixelSize: 11
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: exportController.statusText
            color: exportController.lastError.length > 0 ? "#f7768e" : "#4cc9f0"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    footer: RowLayout {
        spacing: 8

        Item { Layout.fillWidth: true; Layout.preferredHeight: 44 }

        Button {
            visible: exportController.exporting
            text: qsTr("Cancel export")
            onClicked: exportController.cancel()
            contentItem: Text {
                text: parent.text
                color: "#f7768e"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: parent.down ? "#2f2f3d" : "#1c1c26"
                border.color: "#4a2f3d"
                border.width: 1
            }
            Layout.preferredHeight: 30
            Layout.rightMargin: 4
        }

        Button {
            visible: !exportController.exporting
            text: qsTr("Close")
            onClicked: root.close()
            contentItem: Text {
                text: parent.text
                color: "#c8c8d4"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: parent.down ? "#2f2f3d" : "#1c1c26"
                border.color: "#2f2f3d"
                border.width: 1
            }
            Layout.preferredHeight: 30
        }

        Button {
            visible: !exportController.exporting
            text: qsTr("Export…")
            enabled: audioEngine.hasTrack && ffmpegAvailable
            onClicked: saveDialog.open()
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? "#0b0b12" : "#4a4a58"
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: parent.enabled ? (parent.down ? "#3aa8cc" : "#4cc9f0") : "#1c1c26"
            }
            Layout.preferredHeight: 30
            Layout.rightMargin: 14
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Export video to…")
        fileMode: FileDialog.SaveFile
        defaultSuffix: exportController.suggestedExtension()
        currentFile: audioEngine.hasTrack
                     ? "file:///" + audioEngine.trackTitle + "."
                       + exportController.suggestedExtension()
                     : ""
        nameFilters: exportController.videoCodec === "prores"
                     ? [qsTr("QuickTime movie (*.mov)")]
                     : [qsTr("MP4 video (*.mp4)"), qsTr("QuickTime movie (*.mov)")]
        onAccepted: exportController.start(selectedFile)
    }
}
