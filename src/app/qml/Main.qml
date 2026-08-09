import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "panels"

ApplicationWindow {
    id: window

    width: 1440
    height: 900
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    color: "#0e0e14"

    title: qsTr("Music Visualizer — %1%2")
        .arg(project.displayName)
        .arg(project.modified ? " •" : "")

    component ToolbarButton: Button {
        implicitHeight: 28
        padding: 10
        contentItem: Text {
            text: parent.text
            color: parent.enabled ? "#d8d8e4" : "#4a4a58"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 4
            color: parent.down ? "#2f2f3d" : (parent.hovered ? "#26263a" : "#1c1c26")
            border.color: "#2f2f3d"
            border.width: 1
        }
    }

    // ------------------------------------------------------------------ layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // --- toolbar ---
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 6

            ToolbarButton {
                text: qsTr("Import audio…")
                enabled: !audioEngine.loading
                onClicked: audioDialog.open()
            }

            Rectangle { width: 1; Layout.preferredHeight: 20; color: "#2a2a38" }

            ToolbarButton {
                text: qsTr("New")
                onClicked: project.newProject()
            }
            ToolbarButton {
                text: qsTr("Open project…")
                onClicked: openDialog.open()
            }
            ToolbarButton {
                text: qsTr("Save")
                onClicked: {
                    if (!project.saveCurrent())
                        saveDialog.open()
                }
            }
            ToolbarButton {
                text: qsTr("Save as…")
                onClicked: saveDialog.open()
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: audioEngine.loading
                text: audioEngine.loadProgress >= 0
                      ? qsTr("Decoding %1%").arg(Math.round(audioEngine.loadProgress * 100))
                      : qsTr("Decoding…")
                color: "#4cc9f0"
                font.pixelSize: 11
            }

            Text {
                visible: !ffmpegAvailable
                text: qsTr("ffmpeg not found — import and export are unavailable")
                color: "#f7a325"
                font.pixelSize: 11
            }

            ToolbarButton {
                text: qsTr("Export video…")
                enabled: audioEngine.hasTrack && ffmpegAvailable
                onClicked: exportDialog.open()
            }
        }

        // --- canvas + side panel ---
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#08080c"
                border.color: "#22222e"
                border.width: 1
                clip: true

                // Letterbox the canvas at the project's aspect ratio so what the
                // user sees is exactly what gets exported, just smaller.
                Item {
                    id: canvasHolder

                    readonly property real aspect: project.canvasHeight > 0
                        ? project.canvasWidth / project.canvasHeight : 16 / 9
                    readonly property real availW: parent.width - 24
                    readonly property real availH: parent.height - 24

                    width: Math.min(availW, availH * aspect)
                    height: width / aspect
                    anchors.centerIn: parent

                    VisualizerCanvas {
                        anchors.fill: parent
                        layers: layerModel
                        canvasColor: project.canvasColor
                    }

                    // The canvas is often near-black against a near-black box;
                    // this outline makes the exported frame's bounds obvious.
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: "#2c2c40"
                        border.width: 1
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    text: project.canvasWidth + " × " + project.canvasHeight
                    color: "#33334a"
                    font.pixelSize: 10
                }
            }

            // Side panel.
            //
            // fillWidth/fillHeight must be set explicitly here: they default to
            // TRUE for layout items (unlike plain Items), so without this the
            // panel competes with the canvas for horizontal space and wins.
            ColumnLayout {
                Layout.fillWidth: false
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                Layout.minimumWidth: 280
                spacing: 8

                LayerListPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.preferredHeight: 240
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#12121a"
                    border.color: "#22222e"
                    border.width: 1
                    radius: 3

                    PropertyPanel {
                        anchors.fill: parent
                        anchors.margins: 10
                    }
                }
            }
        }

        // --- transport + timeline ---
        TransportBar {
            Layout.fillWidth: true
            Layout.fillHeight: false
        }

        TimelineRuler {
            Layout.fillWidth: true
        }
    }

    // ----------------------------------------------------------------- dialogs
    FileDialog {
        id: audioDialog
        title: qsTr("Import audio track")
        nameFilters: [
            qsTr("Audio files (*.mp3 *.wav *.flac *.aac *.m4a *.ogg *.opus *.wma)"),
            qsTr("All files (*)")
        ]
        onAccepted: audioEngine.importFile(selectedFile)
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open visualizer project")
        nameFilters: [qsTr("Visualizer projects (*.mvproj *.json)")]
        onAccepted: project.load(selectedFile)
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save visualizer project")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "mvproj"
        nameFilters: [qsTr("Visualizer projects (*.mvproj)")]
        onAccepted: project.save(selectedFile)
    }

    ExportDialog {
        id: exportDialog
    }

    // ------------------------------------------------------------------ errors
    Dialog {
        id: errorDialog

        property string message: ""

        anchors.centerIn: parent
        modal: true
        title: qsTr("Something went wrong")
        standardButtons: Dialog.Ok
        width: Math.min(560, window.width - 80)

        contentItem: Text {
            text: errorDialog.message
            color: "#e0e0ea"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        background: Rectangle {
            color: "#16161f"
            border.color: "#33334a"
            border.width: 1
            radius: 4
        }
    }

    function showError(message) {
        errorDialog.message = message
        errorDialog.open()
    }

    Connections {
        target: audioEngine
        function onErrorOccurred(message) { window.showError(message) }
    }
}
