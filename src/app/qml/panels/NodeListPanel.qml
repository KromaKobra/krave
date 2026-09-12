// TEMPORARY. A flat list of the graph's nodes, with no wiring UI at all.
//
// This exists for exactly one reason: the commit that swaps the layer stack for a
// node graph changes every QML file that touched addLayer/moveLayer/selectedIndex,
// and doing that and building the graph canvas in one go would mean a commit nobody
// can run. So the backend lands first with a panel that can select, rename, show
// and hide, and NodeGraphPanel replaces this file in the next one.
//
// What it deliberately cannot do is connect anything, which is why it is temporary
// rather than a fallback worth keeping: in a graph, the edges are the document.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../controls"

Rectangle {
    id: root

    /// nodeId currently being renamed inline, or 0.
    property int renamingId: 0

    color: Theme.surface

    Rectangle {
        anchors.right: parent.right
        height: parent.height
        width: 1
        color: Theme.divider
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------------- header
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.panelHeaderHeight
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            spacing: Theme.space3

            AppIcon { path: Icons.nodes; size: 15; color: Theme.neutral400 }

            Text {
                Layout.fillWidth: true
                text: qsTr("Nodes")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Theme.weightMedium
            }

            Text {
                text: nodeGraph.count
                color: Theme.textGhost
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeMicro
            }
        }

        // --------------------------------------------------------------- list
        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.space2
            Layout.rightMargin: Theme.space2

            clip: true
            model: nodeGraph
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: AppScrollBar {}

            delegate: Item {
                id: row

                required property var nodeObject
                required property string name
                required property string typeId
                required property bool layerVisible

                readonly property bool selected:
                    row.nodeObject && row.nodeObject.nodeId === nodeGraph.selectedNodeId
                readonly property bool renaming:
                    row.nodeObject && row.nodeObject.nodeId === root.renamingId

                width: list.width
                height: Theme.rowHeight

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusMd
                    color: row.selected ? Theme.selectionFill
                                        : (rowHover.hovered ? Theme.hoverFill : "transparent")

                    Behavior on color { ColorAnimation { duration: Theme.durFast } }

                    Rectangle {
                        visible: row.selected
                        width: 2
                        height: parent.height - Theme.space3
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        radius: 1
                        color: Theme.accent
                    }
                }

                HoverHandler { id: rowHover }

                TapHandler {
                    onTapped: nodeGraph.selectedNodeId = row.nodeObject.nodeId
                    onDoubleTapped: {
                        nodeGraph.selectedNodeId = row.nodeObject.nodeId
                        root.renamingId = row.nodeObject.nodeId
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space2
                    spacing: Theme.space3

                    AppIcon {
                        path: NodeTypes.icon(row.typeId)
                        size: 15
                        color: row.selected ? Theme.accent : Theme.neutral400
                        opacity: row.layerVisible ? 1 : 0.4
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        visible: !row.renaming

                        Text {
                            Layout.fillWidth: true
                            text: row.name
                            color: row.layerVisible ? Theme.text : Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            font.weight: row.selected ? Theme.weightMedium : Font.Normal
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: NodeTypes.label(row.typeId).toUpperCase()
                            color: Theme.textGhost
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeKicker
                            font.letterSpacing: 0.4
                            elide: Text.ElideRight
                        }
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        implicitHeight: Theme.controlHeightSm
                        visible: row.renaming
                        font.pixelSize: Theme.fontSizeSmall

                        onVisibleChanged: {
                            if (visible) {
                                text = row.name
                                forceActiveFocus()
                                selectAll()
                            }
                        }
                        onEditingFinished: {
                            if (row.nodeObject && text.trim() !== "")
                                row.nodeObject.name = text.trim()
                            root.renamingId = 0
                        }
                        Keys.onEscapePressed: root.renamingId = 0
                    }

                    IconButton {
                        // Only visual nodes have a visibility flag; for the rest the
                        // property simply is not there.
                        visible: row.nodeObject && row.nodeObject.layerVisible !== undefined
                        iconPath: row.layerVisible ? Icons.eye : Icons.eyeOff
                        iconSize: 14
                        diameter: 24
                        opacity: row.layerVisible ? (rowHover.hovered || row.selected ? 0.9 : 0.45) : 1
                        ink: row.layerVisible ? Theme.textMuted : Theme.accent
                        tip: row.layerVisible ? qsTr("Hide node") : qsTr("Show node")
                        onClicked: row.nodeObject.layerVisible = !row.nodeObject.layerVisible
                    }
                }
            }
        }

        // ------------------------------------------------------------- footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space2
            spacing: Theme.space1

            IconButton {
                iconPath: Icons.plus
                iconSize: 16
                diameter: 32
                outlined: true
                tip: qsTr("Add a node")
                onClicked: addMenu.visible ? addMenu.close() : addMenu.open()
            }

            Item { Layout.fillWidth: true }

            IconButton {
                iconPath: Icons.trash
                iconSize: 14
                tip: qsTr("Delete node")
                enabled: nodeGraph.canRemove(nodeGraph.selectedNodeId)
                onClicked: nodeGraph.removeNode(nodeGraph.selectedNodeId)
            }
        }
    }

    // --------------------------------------------------------------- add menu
    Popup {
        id: addMenu

        x: Theme.space2
        y: root.height - height - 44
        width: 210
        padding: Theme.space1

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surface
            border.width: 1
            border.color: Theme.dividerStrong
        }

        contentItem: ColumnLayout {
            spacing: 1

            Repeater {
                // The C++ port table decides what may be created, so the essential
                // Output and Audio nodes cannot appear here by accident.
                model: nodeGraph.creatableTypes()

                delegate: ItemDelegate {
                    id: option

                    required property string modelData
                    readonly property var entry: NodeTypes.entry(option.modelData)

                    Layout.fillWidth: true
                    implicitHeight: 38
                    padding: Theme.space2

                    AppToolTip {
                        text: option.entry ? option.entry.hint : ""
                        visible: option.hovered
                        x: option.width + Theme.space2
                        y: 0
                        width: Math.min(implicitWidth, 220)
                    }

                    onClicked: {
                        // No canvas to place it on yet, so it lands nowhere in
                        // particular; the graph panel supersedes this.
                        nodeGraph.selectedNodeId = nodeGraph.addNode(option.modelData, 0, 0)
                        addMenu.close()
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space3

                        AppIcon {
                            path: option.entry ? option.entry.icon : Icons.nodes
                            size: 15
                            color: option.hovered ? Theme.accent : Theme.neutral400
                        }

                        Text {
                            Layout.fillWidth: true
                            text: option.entry ? option.entry.title : option.modelData
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                        }
                    }

                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: option.hovered ? Theme.hoverFill : "transparent"
                    }
                }
            }
        }
    }
}
