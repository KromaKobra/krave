import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import krave
import "../controls"

AppDialog {
    id: root

    title: qsTr("Export video")
    width: 460
    closePolicy: exportController.exporting
                 ? Popup.NoAutoClose
                 : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    readonly property var resolutionPresets: [
        { text: "1280 × 720",             value: "1280x720" },
        { text: "1920 × 1080",            value: "1920x1080" },
        { text: "2560 × 1440",            value: "2560x1440" },
        { text: "3840 × 2160",            value: "3840x2160" },
        { text: qsTr("1080 × 1080 — square"),   value: "1080x1080" },
        { text: qsTr("1080 × 1920 — vertical"), value: "1080x1920" }
    ]

    /// The preset matching the current size, so hand-typed dimensions leave the
    /// dropdown showing nothing rather than lying about which preset is active.
    readonly property string currentPreset: exportController.width + "x" + exportController.height

    function formatEta(seconds) {
        if (!(seconds >= 0))
            return qsTr("estimating…")
        const total = Math.round(seconds)
        if (total < 60)
            return qsTr("%1s remaining").arg(total)
        return qsTr("%1m %2s remaining").arg(Math.floor(total / 60)).arg(total % 60)
    }

    contentItem: ColumnLayout {
        spacing: Theme.space4

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space4
            enabled: !exportController.exporting

            EnumCombo {
                label: qsTr("Preset")
                options: root.resolutionPresets
                localValue: root.currentPreset
                onActivated: (value) => {
                    const parts = String(value).split("x")
                    exportController.width = parseInt(parts[0])
                    exportController.height = parseInt(parts[1])
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    text: qsTr("Resolution")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    NumberField {
                        Layout.fillWidth: true
                        value: exportController.width
                        from: 16; to: 7680
                        unit: "px"
                        onEdited: (v) => exportController.width = Math.round(v)
                    }

                    Text {
                        text: "×"
                        color: Theme.textGhost
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }

                    NumberField {
                        Layout.fillWidth: true
                        value: exportController.height
                        from: 16; to: 4320
                        unit: "px"
                        onEdited: (v) => exportController.height = Math.round(v)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    text: qsTr("Frame rate")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    SegmentedControl {
                        Layout.fillWidth: true
                        localValue: exportController.fps
                        options: [
                            { text: "24", value: 24 },
                            { text: "30", value: 30 },
                            { text: "60", value: 60 }
                        ]
                        onActivated: (value) => exportController.fps = value
                    }

                    NumberField {
                        value: exportController.fps
                        from: 1; to: 240
                        unit: "fps"
                        onEdited: (v) => exportController.fps = Math.round(v)
                    }
                }
            }

            EnumCombo {
                label: qsTr("Format")
                target: exportController
                propertyName: "videoCodec"
                options: [
                    { text: qsTr("MP4 — H.264 (widely compatible)"), value: "h264" },
                    { text: qsTr("MOV — ProRes 422 HQ (editing)"), value: "prores" }
                ]
            }

            ParamSlider {
                visible: exportController.videoCodec === "h264"
                label: qsTr("Quality (lower CRF is better)")
                target: exportController
                propertyName: "quality"
                from: 12; to: 30; stepSize: 1
            }

            ToggleRow {
                label: qsTr("Embed the original audio track")
                target: exportController
                propertyName: "includeAudio"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space1
            implicitHeight: 1
            color: Theme.divider
        }

        // ------------------------------------------------------------ summary
        Text {
            Layout.fillWidth: true
            visible: !exportController.exporting
            wrapMode: Text.WordWrap
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            text: {
                if (!audioEngine.hasTrack)
                    return qsTr("Import an audio track first.")
                const frames = Math.ceil(audioEngine.duration * exportController.fps)
                return qsTr("%1 frames at %2 fps — %3s of audio")
                    .arg(frames)
                    .arg(exportController.fps)
                    .arg(audioEngine.duration.toFixed(1))
            }
        }

        // ----------------------------------------------------------- progress
        ColumnLayout {
            Layout.fillWidth: true
            visible: exportController.exporting
            spacing: Theme.space2

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 5
                radius: 2.5
                color: Theme.neutral800

                Rectangle {
                    width: parent.width * exportController.progress
                    height: parent.height
                    radius: parent.radius
                    color: Theme.accent

                    Behavior on width { NumberAnimation { duration: Theme.durNormal } }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("Frame %1 of %2")
                        .arg(exportController.framesDone)
                        .arg(exportController.totalFrames)
                    color: Theme.textMuted
                    font.family: Theme.monoFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.formatEta(exportController.etaSeconds)
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: exportController.statusText
            color: exportController.lastError.length > 0 ? Theme.danger : Theme.accent300
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }

    footer: RowLayout {
        spacing: Theme.space2

        Item { Layout.fillWidth: true; Layout.preferredHeight: 56 }

        AppButton {
            visible: exportController.exporting
            text: qsTr("Cancel export")
            onClicked: exportController.cancel()
        }

        AppButton {
            visible: !exportController.exporting
            text: qsTr("Close")
            variant: "ghost"
            onClicked: root.close()
        }

        AppButton {
            Layout.rightMargin: Theme.space6
            visible: !exportController.exporting
            text: qsTr("Export…")
            iconPath: Icons.exportArrow
            variant: "primary"
            enabled: audioEngine.hasTrack && ffmpegAvailable
            onClicked: saveDialog.open()
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
