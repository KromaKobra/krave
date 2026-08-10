import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import "controls"
import "panels"

// The editor shell: an application bar over three resizable panes, with the
// transport docked along the bottom.
//
// Layout only. Every action the bar or a shortcut raises is funnelled through the
// functions below, so the menu, the keyboard and the buttons cannot drift apart.
ApplicationWindow {
    id: window

    width: 1500
    height: 940
    minimumWidth: 1080
    minimumHeight: 680
    visible: true
    color: Theme.bg

    title: qsTr("Music Visualizer — %1%2")
        .arg(project.displayName)
        .arg(project.modified ? " •" : "")

    // Window shortcuts are matched before the focused item sees the key, so the
    // bare ones -- Space, Delete -- have to stand down while a field is being
    // typed into or they eat the keystroke.
    readonly property bool typing:
        activeFocusItem !== null && activeFocusItem.inputMethodComposing !== undefined

    // ----------------------------------------------------------------- actions
    function saveProject() {
        if (!project.saveCurrent())
            saveDialog.open()
    }

    function showError(message) {
        errorDialog.message = message
        errorDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            Layout.fillWidth: true

            onNewProjectRequested: project.newProject()
            onOpenProjectRequested: openDialog.open()
            onImportAudioRequested: audioDialog.open()
            onSaveRequested: window.saveProject()
            onSaveAsRequested: saveDialog.open()
            onExportRequested: exportDialog.open()
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 5
                color: SplitHandle.pressed ? Theme.accent
                     : (SplitHandle.hovered ? Theme.withAlpha(Theme.accent, 0.4) : "transparent")

                Behavior on color { ColorAnimation { duration: Theme.durFast } }
            }

            LayerListPanel {
                SplitView.preferredWidth: Theme.leftPanelWidth
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 420
            }

            CanvasStage {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320
            }

            PropertyPanel {
                SplitView.preferredWidth: Theme.rightPanelWidth
                SplitView.minimumWidth: 280
                SplitView.maximumWidth: 520
            }
        }

        TimelinePanel {
            Layout.fillWidth: true
        }
    }

    // --------------------------------------------------------------- shortcuts
    Shortcut {
        sequence: "Space"
        enabled: audioEngine.hasTrack && !window.typing
        onActivated: audioEngine.togglePlay()
    }
    Shortcut { sequence: StandardKey.New;  onActivated: project.newProject() }
    Shortcut { sequence: StandardKey.Open; onActivated: openDialog.open() }
    Shortcut { sequence: StandardKey.Save; onActivated: window.saveProject() }
    Shortcut { sequence: StandardKey.SaveAs; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+I"; onActivated: audioDialog.open() }
    Shortcut {
        sequence: "Ctrl+E"
        enabled: audioEngine.hasTrack && ffmpegAvailable
        onActivated: exportDialog.open()
    }
    Shortcut {
        sequence: StandardKey.Delete
        enabled: layerModel.selectedIndex >= 0 && !window.typing
        onActivated: layerModel.removeLayer(layerModel.selectedIndex)
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
    AppDialog {
        id: errorDialog

        property string message: ""

        title: qsTr("Something went wrong")
        width: Math.min(520, window.width - 120)

        contentItem: Text {
            text: errorDialog.message
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            wrapMode: Text.WordWrap
        }

        footer: RowLayout {
            Item { Layout.fillWidth: true; Layout.preferredHeight: 56 }

            AppButton {
                Layout.rightMargin: Theme.space6
                text: qsTr("Close")
                variant: "primary"
                onClicked: errorDialog.close()
            }
        }
    }

    Connections {
        target: audioEngine
        function onErrorOccurred(message) { window.showError(message) }
    }
}
