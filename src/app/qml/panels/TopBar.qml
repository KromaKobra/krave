import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../controls"

// The application bar: identity on the left, document actions on the right.
//
// It owns no state -- every button raises a signal that Main handles, so the
// dialogs and the shortcuts share one set of entry points.
Rectangle {
    id: root

    signal newProjectRequested()
    signal openProjectRequested()
    signal importAudioRequested()
    signal saveRequested()
    signal saveAsRequested()
    signal exportRequested()

    implicitHeight: Theme.topBarHeight
    color: Theme.surface

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        spacing: Theme.space4

        // ------------------------------------------------------------- brand
        RowLayout {
            spacing: Theme.space3

            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: Theme.radiusSm
                color: Theme.withAlpha(Theme.accent, 0.2)

                AppIcon {
                    anchors.centerIn: parent
                    path: Icons.bars
                    size: 15
                    color: Theme.accent
                }
            }

            Text {
                text: qsTr("Music Visualizer")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 14
                font.weight: Theme.weightMedium
            }
        }

        Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: Theme.divider }

        // ------------------------------------------------------------ document
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                text: project.displayName
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideMiddle
                Layout.maximumWidth: 220
            }

            Rectangle {
                visible: project.modified
                Layout.preferredWidth: 6
                Layout.preferredHeight: 6
                radius: 3
                color: Theme.accent

                HoverHandler { id: unsavedHover }

                AppToolTip {
                    text: qsTr("Unsaved changes")
                    visible: unsavedHover.hovered
                }
            }

            Item { Layout.fillWidth: true }

            // --------------------------------------------------------- status
            RowLayout {
                visible: audioEngine.loading
                spacing: Theme.space2

                AppIcon {
                    path: Icons.spinner
                    size: 13
                    color: Theme.accent

                    RotationAnimation on rotation {
                        running: audioEngine.loading
                        loops: Animation.Infinite
                        from: 0; to: 360; duration: 900
                    }
                }

                Text {
                    text: audioEngine.loadProgress >= 0
                          ? qsTr("Decoding %1%").arg(Math.round(audioEngine.loadProgress * 100))
                          : qsTr("Decoding…")
                    color: Theme.accent300
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            RowLayout {
                visible: !ffmpegAvailable
                spacing: Theme.space2

                AppIcon { path: Icons.warning; size: 14; color: Theme.warning }

                Text {
                    text: qsTr("ffmpeg not found — import and export are unavailable")
                    color: Theme.warning
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        // ------------------------------------------------------------ actions
        RowLayout {
            spacing: Theme.space1

            AppButton {
                text: qsTr("New")
                iconPath: Icons.filePlus
                dense: true
                onClicked: root.newProjectRequested()
            }

            AppButton {
                text: qsTr("Open")
                iconPath: Icons.folderOpen
                dense: true
                onClicked: root.openProjectRequested()
            }

            AppButton {
                text: qsTr("Import audio")
                iconPath: Icons.importArrow
                dense: true
                enabled: !audioEngine.loading
                onClicked: root.importAudioRequested()
            }
        }

        Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: Theme.divider }

        RowLayout {
            spacing: Theme.space1

            AppButton {
                text: qsTr("Save")
                iconPath: Icons.save
                dense: true
                onClicked: root.saveRequested()
            }

            AppButton {
                text: qsTr("Save as…")
                variant: "ghost"
                dense: true
                onClicked: root.saveAsRequested()
            }

            AppButton {
                text: qsTr("Export video")
                iconPath: Icons.exportArrow
                variant: "primary"
                dense: true
                enabled: audioEngine.hasTrack && ffmpegAvailable
                onClicked: root.exportRequested()
            }
        }
    }
}
