// The layer stack. Displayed top-of-stack first, which is the reverse of the
// model order (index 0 is the bottom layer, painted first).
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    spacing: 6

    function modelRow(visualRow) {
        return layerModel.count - 1 - visualRow
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: qsTr("Layers")
            color: "#e0e0ea"
            font.pixelSize: 12
            font.bold: true
            Layout.fillWidth: true
        }

        component MiniButton: Button {
            implicitWidth: 24
            implicitHeight: 22
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? "#c8c8d4" : "#4a4a58"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 3
                color: parent.down ? "#2f2f3d" : (parent.hovered ? "#26263a" : "#1c1c26")
                border.color: "#2f2f3d"
                border.width: 1
            }
        }

        MiniButton {
            text: "▲"
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Move layer up")
            enabled: layerModel.selectedIndex >= 0
                     && layerModel.selectedIndex < layerModel.count - 1
            onClicked: layerModel.moveLayer(layerModel.selectedIndex,
                                            layerModel.selectedIndex + 1)
        }

        MiniButton {
            text: "▼"
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Move layer down")
            enabled: layerModel.selectedIndex > 0
            onClicked: layerModel.moveLayer(layerModel.selectedIndex,
                                            layerModel.selectedIndex - 1)
        }

        MiniButton {
            text: "✕"
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Delete layer")
            enabled: layerModel.selectedIndex >= 0
            onClicked: layerModel.removeLayer(layerModel.selectedIndex)
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 110
        color: "#12121a"
        border.color: "#22222e"
        border.width: 1
        radius: 3

        ListView {
            id: list

            // BottomToTop puts the top of the z-stack at the top of the list,
            // matching how layer stacks are conventionally shown. It also packs
            // items against the BOTTOM of the view, so the view is sized to its
            // content to keep a short list from floating away from the header.
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: Math.min(contentHeight, parent.height - 2)

            clip: true
            model: layerModel
            verticalLayoutDirection: ListView.BottomToTop

            delegate: Rectangle {
                id: row

                required property int index
                required property string name
                required property string typeId
                required property bool layerVisible

                width: list.width
                height: 28
                color: index === layerModel.selectedIndex ? "#252538" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 6

                    Text {
                        text: row.layerVisible ? "◉" : "○"
                        color: row.layerVisible ? "#4cc9f0" : "#55556a"
                        font.pixelSize: 12

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const layer = layerModel.at(row.index)
                                if (layer)
                                    layer.layerVisible = !layer.layerVisible
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: row.name
                        color: "#e0e0ea"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Text {
                        text: row.typeId
                        color: "#55556a"
                        font.pixelSize: 10
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    z: -1
                    onClicked: layerModel.selectedIndex = row.index
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        component AddButton: Button {
            implicitHeight: 24
            contentItem: Text {
                text: parent.text
                color: "#c8c8d4"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 3
                color: parent.down ? "#2f2f3d" : (parent.hovered ? "#26263a" : "#1c1c26")
                border.color: "#2f2f3d"
                border.width: 1
            }
        }

        AddButton {
            Layout.fillWidth: true
            text: qsTr("+ Bars")
            onClicked: layerModel.addLayer("bars")
        }

        AddButton {
            Layout.fillWidth: true
            text: qsTr("+ Text")
            onClicked: layerModel.addLayer("text")
        }

        AddButton {
            Layout.fillWidth: true
            text: qsTr("+ Particles")
            onClicked: layerModel.addLayer("particles")
        }

        AddButton {
            Layout.fillWidth: true
            text: qsTr("+ BG")
            onClicked: layerModel.addLayer("background")
        }
    }
}
