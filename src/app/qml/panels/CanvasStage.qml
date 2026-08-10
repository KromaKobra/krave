// The preview: a letterboxed VisualizerCanvas at the project's aspect ratio, so
// what the user sees is exactly what gets exported, only smaller.
//
// Nothing in this file draws into the canvas item itself -- the zoom controls and
// the composition guides sit above it -- so the export path renders the same tree
// without any editor chrome leaking into the frame.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import MusicVisualizer
import "../controls"

Item {
    id: root

    readonly property real aspect: project.canvasHeight > 0
        ? project.canvasWidth / project.canvasHeight : 16 / 9
    readonly property int margin: Theme.space8

    property string zoom: "fit"
    property bool guidesVisible: false

    readonly property real fittedWidth: Math.max(
        80, Math.min(view.width - margin * 2, (view.height - margin * 2) * aspect))
    readonly property real frameWidth: zoom === "fit" ? fittedWidth : project.canvasWidth
    readonly property real frameHeight: frameWidth / aspect

    Rectangle {
        anchors.fill: parent
        color: Theme.stage
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------------ toolbar
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            RowLayout {
                anchors.centerIn: parent
                spacing: Theme.space3

                SegmentedControl {
                    Layout.fillWidth: false
                    localValue: root.zoom
                    options: [
                        { text: qsTr("Fit"), value: "fit" },
                        { text: qsTr("100%"), value: "full" }
                    ]
                    onActivated: (value) => root.zoom = value
                }

                Tag { text: project.canvasWidth + " × " + project.canvasHeight }

                Tag {
                    variant: "outline"
                    text: qsTr("%1 fps").arg(exportController.fps)
                }
            }

            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space4
                anchors.verticalCenter: parent.verticalCenter
                iconPath: Icons.frameCorners
                active: root.guidesVisible
                tip: qsTr("Composition guides")
                onClicked: root.guidesVisible = !root.guidesVisible
            }
        }

        // ------------------------------------------------------------- canvas
        Flickable {
            id: view

            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            contentWidth: Math.max(width, root.frameWidth + root.margin * 2)
            contentHeight: Math.max(height, root.frameHeight + root.margin * 2)
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: AppScrollBar {}
            ScrollBar.horizontal: AppScrollBar { orientation: Qt.Horizontal }

            Item {
                width: view.contentWidth
                height: view.contentHeight

                Item {
                    id: frame

                    anchors.centerIn: parent
                    width: root.frameWidth
                    height: root.frameHeight

                    VisualizerCanvas {
                        anchors.fill: parent
                        layers: layerModel
                        canvasColor: project.canvasColor
                    }

                    // The canvas is often near-black against a near-black stage;
                    // this hairline makes the exported frame's bounds obvious.
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.divider
                    }

                    // ------------------------------------------------- guides
                    Item {
                        anchors.fill: parent
                        visible: root.guidesVisible

                        Repeater {
                            model: [1 / 3, 2 / 3]
                            delegate: Rectangle {
                                required property real modelData
                                x: Math.round(frame.width * modelData)
                                width: 1
                                height: frame.height
                                color: Theme.withAlpha(Theme.text, 0.18)
                            }
                        }

                        Repeater {
                            model: [1 / 3, 2 / 3]
                            delegate: Rectangle {
                                required property real modelData
                                y: Math.round(frame.height * modelData)
                                height: 1
                                width: frame.width
                                color: Theme.withAlpha(Theme.text, 0.18)
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: Math.round(Math.min(frame.width, frame.height) * 0.05)
                            color: "transparent"
                            radius: Theme.radiusSm
                            border.width: 1
                            border.color: Theme.withAlpha(Theme.accent, 0.45)
                        }
                    }
                }
            }
        }

        // ------------------------------------------------------------ caption
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.space3
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textGhost
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMicro
            text: {
                const size = project.canvasWidth + " × " + project.canvasHeight
                const layer = layerModel.selectedLayer
                return layer ? qsTr("%1 · %2 selected").arg(size).arg(layer.name) : size
            }
        }
    }
}
