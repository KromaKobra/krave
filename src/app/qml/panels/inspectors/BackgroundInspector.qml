// Fill colour and still image for a background layer.
//
// The image FileDialog lives here rather than in PropertyPanel: it is the only
// thing that opens it, and keeping it alongside its button means the panel has no
// id for this block to reach up into.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import krave
import "../../controls"

ColumnLayout {
    id: root

    /// The selected Layer, or null.
    property var node: null
    /// Its typeId, hoisted so the visibility test costs no null check.
    property string nodeType: ""

    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "background"

    SectionHeader { text: qsTr("Fill") }

    ColorField {
        label: qsTr("Colour"); target: root.node; propertyName: "color"
        info: qsTr("The flat colour behind everything else. An image, if one "
                   + "is set, covers it.")
    }

    SectionHeader { text: qsTr("Image") }

    Text {
        Layout.fillWidth: true
        visible: root.node && root.node.imageSource !== undefined
                 && root.node.imageSource !== ""
        text: root.node && root.node.imageSource
              ? decodeURIComponent(String(root.node.imageSource).split("/").pop())
              : ""
        color: Theme.textGhost
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeKicker
        elide: Text.ElideMiddle
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2

        AppButton {
            Layout.fillWidth: true
            dense: true
            iconPath: Icons.image
            text: root.node && root.node.imageSource
                  ? qsTr("Change…") : qsTr("Choose image…")
            onClicked: imageDialog.open()
        }

        IconButton {
            iconPath: Icons.close
            iconSize: 13
            visible: root.node && root.node.imageSource !== undefined
                     && root.node.imageSource !== ""
            tip: qsTr("Clear image")
            onClicked: if (root.node) root.node.imageSource = ""
        }
    }

    SegmentedControl {
        label: qsTr("Fill mode"); target: root.node; propertyName: "fillMode"
        info: qsTr("How the image meets the canvas: Crop fills it and loses "
                   + "the overhang, Fit shows all of it and leaves bars, "
                   + "Stretch distorts it to fit exactly.")
        options: [
            { text: qsTr("Crop"), value: "crop" },
            { text: qsTr("Fit"), value: "fit" },
            { text: qsTr("Stretch"), value: "stretch" }
        ]
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Choose a background image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: if (root.node) root.node.imageSource = selectedFile
    }
}
