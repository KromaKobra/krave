// The layer stack.
//
// Shown top-of-stack first, which is the reverse of the model order (index 0 is
// the bottom layer, painted first). Everything that changes the stack lives in
// the footer, so the list itself stays a quiet column of names.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import MusicVisualizer
import "../controls"

Rectangle {
    id: root

    /// Index currently being renamed inline, or -1.
    property int renamingIndex: -1

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

            AppIcon { path: Icons.stack; size: 15; color: Theme.neutral400 }

            Text {
                Layout.fillWidth: true
                text: qsTr("Layers")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Theme.weightMedium
            }

            Text {
                text: layerModel.count
                color: Theme.textGhost
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeMicro
            }
        }

        // --------------------------------------------------------------- list
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.space2
            Layout.rightMargin: Theme.space2
            Layout.bottomMargin: Theme.space2

            ListView {
                id: list

                // BottomToTop packs items against the BOTTOM of the view, so the view
                // is sized to its content to keep a short stack from floating away
                // from the header. The height is derived from the row count rather
                // than from contentHeight: a view only realizes the delegates that
                // fit, so sizing to contentHeight would make height depend on itself.
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.min(layerModel.count * (Theme.rowHeight + spacing), parent.height)

                clip: true
                model: layerModel
                // BottomToTop puts the top of the z-stack at the top of the list,
                // which is how layer stacks are conventionally read.
                verticalLayoutDirection: ListView.BottomToTop
                spacing: 1
                boundsBehavior: Flickable.StopAtBounds
                currentIndex: layerModel.selectedIndex

                ScrollBar.vertical: AppScrollBar {}

                delegate: Item {
                    id: row

                    required property int index
                    required property string name
                    required property string typeId
                    required property bool layerVisible

                    readonly property bool selected: index === layerModel.selectedIndex
                    readonly property bool renaming: index === root.renamingIndex

                    width: list.width
                    height: Theme.rowHeight

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusMd
                        color: row.selected ? Theme.selectionFill
                                            : (rowHover.hovered ? Theme.hoverFill : "transparent")

                        Behavior on color { ColorAnimation { duration: Theme.durFast } }

                        // The selected row carries the accent as a mark on its edge
                        // rather than as a filled highlight.
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
                        onTapped: layerModel.selectedIndex = row.index
                        onDoubleTapped: {
                            layerModel.selectedIndex = row.index
                            root.renamingIndex = row.index
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space3
                        anchors.rightMargin: Theme.space2
                        spacing: Theme.space3

                        AppIcon {
                            path: LayerTypes.icon(row.typeId)
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
                                text: LayerTypes.label(row.typeId).toUpperCase()
                                color: Theme.textGhost
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeKicker
                                font.letterSpacing: 0.4
                                elide: Text.ElideRight
                            }
                        }

                        AppTextField {
                            id: renameField

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
                                const layer = layerModel.at(row.index)
                                if (layer && text.trim() !== "")
                                    layer.name = text.trim()
                                root.renamingIndex = -1
                            }
                            Keys.onEscapePressed: root.renamingIndex = -1
                        }

                        IconButton {
                            iconPath: row.layerVisible ? Icons.eye : Icons.eyeOff
                            iconSize: 14
                            diameter: 24
                            opacity: row.layerVisible ? (rowHover.hovered || row.selected ? 0.9 : 0.45) : 1
                            ink: row.layerVisible ? Theme.textMuted : Theme.accent
                            tip: row.layerVisible ? qsTr("Hide layer") : qsTr("Show layer")
                            onClicked: {
                                const layer = layerModel.at(row.index)
                                if (layer)
                                    layer.layerVisible = !layer.layerVisible
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------- empty state
            Text {
                anchors.centerIn: parent
                width: parent.width - Theme.space8
                visible: layerModel.count === 0
                text: qsTr("No layers yet.\nAdd one below to start building the scene.")
                color: Theme.textGhost
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
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
                id: addButton
                iconPath: Icons.plus
                iconSize: 16
                diameter: 32
                outlined: true
                tip: qsTr("Add a layer")
                onClicked: addMenu.visible ? addMenu.close() : addMenu.open()
            }

            Item { Layout.fillWidth: true }

            IconButton {
                iconPath: Icons.arrowUp
                iconSize: 14
                tip: qsTr("Move layer up")
                enabled: layerModel.selectedIndex >= 0
                         && layerModel.selectedIndex < layerModel.count - 1
                onClicked: layerModel.moveLayer(layerModel.selectedIndex,
                                                layerModel.selectedIndex + 1)
            }

            IconButton {
                iconPath: Icons.arrowDown
                iconSize: 14
                tip: qsTr("Move layer down")
                enabled: layerModel.selectedIndex > 0
                onClicked: layerModel.moveLayer(layerModel.selectedIndex,
                                                layerModel.selectedIndex - 1)
            }

            IconButton {
                iconPath: Icons.trash
                iconSize: 14
                tip: qsTr("Delete layer")
                enabled: layerModel.selectedIndex >= 0
                onClicked: layerModel.removeLayer(layerModel.selectedIndex)
            }
        }
    }

    // ------------------------------------------------------------- add menu
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
                model: LayerTypes.all

                delegate: ItemDelegate {
                    id: option

                    required property var modelData

                    Layout.fillWidth: true
                    implicitHeight: 38
                    padding: Theme.space2

                    AppToolTip {
                        text: option.modelData.hint
                        visible: option.hovered
                        // The menu is anchored to the bottom-left of the panel, so
                        // the blurb goes beside the row rather than under it.
                        x: option.width + Theme.space2
                        y: 0
                        width: Math.min(implicitWidth, 220)
                    }

                    onClicked: {
                        layerModel.selectedIndex = layerModel.addLayer(option.modelData.typeId)
                        addMenu.close()
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space3

                        AppIcon {
                            path: option.modelData.icon
                            size: 15
                            color: option.hovered ? Theme.accent : Theme.neutral400
                        }

                        Text {
                            Layout.fillWidth: true
                            text: option.modelData.title
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
