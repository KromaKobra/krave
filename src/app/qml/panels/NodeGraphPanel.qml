// The node editor: header, canvas, footer.
//
// Chrome only. Everything about how the graph looks and behaves lives in graph/.
// The footer has no up/down/delete buttons: ordering is topology now, and delete is
// the Delete key, gated on nodeGraph.canRemove.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../controls"
import "../graph"

Rectangle {
    id: root

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
                text: qsTr("Graph")
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        // ------------------------------------------------------------- canvas
        GraphCanvas {
            id: canvas

            Layout.fillWidth: true
            Layout.fillHeight: true

            onRenameRequested: (nodeId) => renameDialog.beginFor(nodeId)
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
                iconPath: Icons.nodes
                iconSize: 14
                tip: qsTr("Arrange the graph in rows")
                onClicked: {
                    nodeGraph.autoLayout()
                    canvas.resetView()
                }
            }

            IconButton {
                iconPath: Icons.frameCorners
                iconSize: 14
                tip: qsTr("Reset the view")
                onClicked: canvas.resetView()
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
                // Output and Audio nodes cannot appear here by accident -- and
                // NodeTypes.qml only has to supply the icon and the blurb.
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
                        // The menu is anchored to the bottom-left of the panel, so
                        // the blurb goes beside the row rather than under it.
                        x: option.width + Theme.space2
                        y: 0
                        width: Math.min(implicitWidth, 220)
                    }

                    onClicked: {
                        // Placed at the centre of whatever the user is looking at,
                        // not at the graph origin they may have panned away from.
                        const at = canvas.viewportCentreInGraph()
                        nodeGraph.selectedNodeId =
                            nodeGraph.addNode(option.modelData, at.x, at.y)
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

    // ----------------------------------------------------------------- rename
    //
    // A popup rather than an inline field: a node card is 176 px wide with an icon
    // and a kicker in it, and an editor squeezed in there would be worse than this.
    Popup {
        id: renameDialog

        property int nodeId: 0

        function beginFor(id) {
            renameDialog.nodeId = id
            const node = nodeGraph.nodeById(id)
            nameField.text = node ? node.name : ""
            renameDialog.open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }

        function commit() {
            const node = nodeGraph.nodeById(renameDialog.nodeId)
            if (node && nameField.text.trim() !== "")
                node.name = nameField.text.trim()
            renameDialog.close()
        }

        anchors.centerIn: parent
        width: Math.min(260, root.width - Theme.space8)
        padding: Theme.space3
        modal: true

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surface
            border.width: 1
            border.color: Theme.dividerStrong
        }

        contentItem: ColumnLayout {
            spacing: Theme.space2

            ParamLabel { text: qsTr("Node name") }

            AppTextField {
                id: nameField

                Layout.fillWidth: true
                onEditingFinished: renameDialog.commit()
                Keys.onEscapePressed: renameDialog.close()
            }
        }
    }
}
