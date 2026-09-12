// The preview: a letterboxed VisualizerCanvas at the project's aspect ratio, so
// what the user sees is exactly what gets exported, only smaller.
//
// Nothing in this file draws into the canvas item itself -- the zoom controls and
// the composition guides sit above it -- so the export path renders the same tree
// without any editor chrome leaking into the frame.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
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
                        layers: nodeGraph.renderList
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

                    // -------------------------------------- blur mask guide
                    //
                    // Where the selected blur node's circle falls, drawn ABOVE
                    // the canvas item like every other guide here and therefore
                    // impossible to leak into an exported frame.
                    //
                    // Shown whenever a blur with its circle on is selected, not
                    // gated on the guides toggle: those are permanent framing
                    // aids, this one is the readout for a control the user is
                    // actively dragging.
                    //
                    // Nothing here is draggable. No gizmo of any kind exists in
                    // this editor yet, and the circle is moved and sized from
                    // the inspector's sliders.
                    //
                    // Drawn in canvas coordinates, so it shows where the mask
                    // sits BEFORE any enclosing Transform -- which is where it
                    // is authored. A blur inside a scaled transform has its
                    // circle scaled with the group (see LayerFrame), and the
                    // guide does not follow that; it would have to be drawn per
                    // layer rather than per node to do so.
                    Item {
                        id: maskGuide

                        readonly property var node: {
                            const n = nodeGraph.selectedNode
                            return (n && n.typeId === "blur" && n.maskEnabled) ? n : null
                        }

                        anchors.fill: parent
                        visible: maskGuide.node !== null

                        // Plain arithmetic over frameWidth/frameHeight rather
                        // than mapToItem: only the former registers as a QML
                        // dependency, so only the former follows a slider.
                        Repeater {
                            // The radius itself, then the two ends of the soft
                            // edge -- which is where the falloff actually
                            // starts and stops mattering.
                            model: [
                                { scale: 1, alpha: 0.55 },
                                { scale: 1 - (maskGuide.node ? maskGuide.node.maskSoftness : 0),
                                  alpha: 0.22 },
                                { scale: 1 + (maskGuide.node ? maskGuide.node.maskSoftness : 0),
                                  alpha: 0.22 }
                            ]

                            delegate: Rectangle {
                                required property var modelData

                                // maskRadius is a fraction of canvas HEIGHT, so
                                // the guide is round at any aspect ratio -- the
                                // same convention the shader's mask uses.
                                readonly property real r: Math.max(
                                    0, (maskGuide.node ? maskGuide.node.maskRadius : 0)
                                       * modelData.scale * root.frameHeight)

                                x: (maskGuide.node ? maskGuide.node.maskX : 0.5)
                                   * root.frameWidth - r
                                y: (maskGuide.node ? maskGuide.node.maskY : 0.5)
                                   * root.frameHeight - r
                                width: r * 2
                                height: r * 2
                                radius: r
                                color: "transparent"
                                border.width: 1
                                border.color: Theme.withAlpha(Theme.accent, modelData.alpha)
                            }
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
                const node = nodeGraph.selectedNode
                return node ? qsTr("%1 · %2 selected").arg(size).arg(node.name) : size
            }
        }
    }

    // ---------------------------------------------------------- fps readout
    //
    // Measured from frames the scene graph actually presented, not from the
    // spectrum tick -- the point is to see when rendering fails to keep up with
    // the 16 ms preview cadence. Sits in the stage's corner rather than the
    // canvas frame's so it stays put when the canvas is zoomed or scrolled.
    Item {
        id: fpsMeter

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space3
        implicitWidth: fpsPill.width
        implicitHeight: fpsPill.height

        property int frames: 0
        property real fps: 0

        FrameAnimation {
            running: true
            onTriggered: fpsMeter.frames++
        }

        // Counting over a window rather than inverting a single frame time: one
        // long frame should not make the number leap about.
        Timer {
            interval: 500
            repeat: true
            running: true
            onTriggered: {
                fpsMeter.fps = fpsMeter.frames * 1000 / interval
                fpsMeter.frames = 0
            }
        }

        Rectangle {
            id: fpsPill

            width: fpsLabel.implicitWidth + Theme.space3 * 2
            height: fpsLabel.implicitHeight + Theme.space1 * 2
            radius: Theme.radiusSm
            color: Theme.withAlpha(Theme.bg, 0.72)
            border.width: 1
            border.color: Theme.divider

            Text {
                id: fpsLabel

                anchors.centerIn: parent
                text: qsTr("%1 fps").arg(Math.round(fpsMeter.fps))
                color: Theme.textMuted
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeKicker
            }
        }
    }
}
