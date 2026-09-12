// Editor for the selected node.
//
// Every control writes directly to a Q_PROPERTY on the C++ Node object. Because
// the canvas binds to those same properties, edits are visible on the next frame
// -- there is no apply step and no reload.
//
// This file holds the header and the sections the visual node types share. The
// per-type blocks live one to a file in inspectors/.
//
// The sections stay instantiated and hide themselves, which is why each control
// tolerates being pointed at a node that has no such property: the alternative,
// tearing the panel down on every selection change, costs more than the handful of
// idle bindings does. The inspectors are plain ColumnLayouts, not Loaders.
//
// The common sections are gated on `root.placed`. Placement and compositing live
// on VisualNode now, so a selected separator, audio or output node genuinely has
// no posX -- and the "every control tolerates a missing property" tolerance would
// otherwise leave seven dead sliders on screen, which is exactly what it used to
// do for an audio provider.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../controls"
import "inspectors"

Rectangle {
    id: root

    readonly property var selected: nodeGraph.selectedNode
    readonly property string selectedType: selected ? selected.typeId : ""
    /// True when the selection is a VisualNode, i.e. has placement and blending.
    readonly property bool placed: selected !== null && selected.posX !== undefined

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
                        path: NodeTypes.icon(root.selectedType)
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
                        text: NodeTypes.label(root.selectedType)
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
                    text: qsTr("Select a node to edit its properties.")
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

                // ------------------------------------------------------- node
                SectionHeader { text: qsTr("Node"); leading: true }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    ParamLabel {
                        text: qsTr("Name")
                        info: qsTr("What this node is called in the graph. It has no effect on "
                                   + "what gets drawn.")
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        text: root.selected ? root.selected.name : ""
                        onTextEdited: if (root.selected) root.selected.name = text
                    }
                }

                // ------------------------------------------- layer and placement
                //
                // Only VisualNodes have any of this. A separator, the audio source
                // and the output node have no box on the canvas at all.
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    visible: root.placed

                    ToggleRow {
                        label: qsTr("Visible")
                        info: qsTr("Hides the layer without deleting it. Hidden layers are left "
                                   + "out of the export too.")
                        target: root.selected
                        propertyName: "layerVisible"
                    }

                    ParamSlider {
                        label: qsTr("Opacity")
                        info: qsTr("How much of the layer shows through: invisible at 0, solid at 1.")
                        target: root.selected; propertyName: "layerOpacity"
                        from: 0; to: 1
                    }

                    SegmentedControl {
                        label: qsTr("Blend mode"); target: root.selected; propertyName: "blendMode"
                        info: qsTr("How this layer's colour combines with whatever is beneath it.")
                        details: qsTr("Normal simply covers what is underneath, using the layer's "
                                      + "opacity to mix.\n\n"
                                      + "Add sums the two colours. Overlapping bright areas climb "
                                      + "toward white, which is what makes bars over a dark "
                                      + "background read as glowing rather than as flat shapes. "
                                      + "Nothing can get darker than what was already there.\n\n"
                                      + "Screen is a gentler version of the same idea: it "
                                      + "brightens, but rolls off as it approaches white instead "
                                      + "of clipping there. Reach for it when Add blows out the "
                                      + "peaks.")
                        options: [
                            { text: qsTr("Normal"), value: "normal" },
                            { text: qsTr("Add"), value: "add" },
                            { text: qsTr("Screen"), value: "screen" }
                        ]
                    }

                    // -------------------------------------------------- placement
                    SectionHeader { text: qsTr("Placement") }

                    ParamSlider {
                        label: qsTr("Centre X")
                        info: qsTr("Where the layer's centre sits across the canvas: 0 is the left "
                                   + "edge, 1 the right.")
                        target: root.selected; propertyName: "posX"
                        from: -0.5; to: 1.5
                    }
                    ParamSlider {
                        label: qsTr("Centre Y")
                        info: qsTr("Where the layer's centre sits down the canvas: 0 is the top, "
                                   + "1 the bottom.")
                        target: root.selected; propertyName: "posY"
                        from: -0.5; to: 1.5
                    }
                    ParamSlider {
                        label: qsTr("Width")
                        info: qsTr("Layer width as a fraction of the canvas, so 1 spans it "
                                   + "completely and the framing survives a change of resolution.")
                        target: root.selected; propertyName: "sizeW"
                        from: 0; to: 2
                    }
                    ParamSlider {
                        label: qsTr("Height")
                        info: qsTr("Layer height as a fraction of the canvas, so 1 spans it "
                                   + "completely and the framing survives a change of resolution.")
                        target: root.selected; propertyName: "sizeH"
                        from: 0; to: 2
                    }
                    ParamSlider {
                        label: qsTr("Rotation")
                        info: qsTr("Turns the layer about its own centre. Negative goes "
                                   + "anticlockwise.")
                        target: root.selected; propertyName: "layerRotation"
                        from: -180; to: 180; stepSize: 1; unit: "°"
                    }
                }

                // ------------------------------------------------- per type
                //
                // One file per node type, all instantiated and all gated on
                // `visible`. See the header comment for why they are not
                // Loaders.
                SpectrumInspector   { node: root.selected; nodeType: root.selectedType }
                SeparatorInspector  { node: root.selected; nodeType: root.selectedType }
                TextInspector       { node: root.selected; nodeType: root.selectedType }
                BackgroundInspector { node: root.selected; nodeType: root.selectedType }
                ImageInspector      { node: root.selected; nodeType: root.selectedType }
                TransformInspector  { node: root.selected; nodeType: root.selectedType }
                // Merge has no properties at all -- the compositing is its port
                // order -- so the shared Node section above is its whole panel.
            }
        }
    }
}
