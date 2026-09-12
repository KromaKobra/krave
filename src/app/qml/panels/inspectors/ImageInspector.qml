// File choice, fill mode and crop for an image node.
//
// The FileDialog lives here rather than in PropertyPanel: it is the only thing
// that opens it, and keeping it alongside its button means the panel has no id
// for this block to reach up into.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import krave
import "../../controls"

ColumnLayout {
    id: root

    /// The selected Node, or null.
    property var node: null
    /// Its typeId, hoisted so the visibility test costs no null check.
    property string nodeType: ""

    readonly property string source: (root.node && root.node.source !== undefined)
                                     ? root.node.source : ""

    /// The inspectors stay instantiated while hidden, so this is routinely
    /// pointed at a node with no crop at all. Fall back rather than letting
    /// `undefined` reach the outline's geometry as a NaN.
    function crop(name, fallback) {
        if (!root.node)
            return fallback
        const v = root.node[name]
        return v === undefined ? fallback : v
    }

    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "image"

    SectionHeader { text: qsTr("File") }

    Text {
        Layout.fillWidth: true
        visible: root.source !== ""
        // Paths are stored absolute and are usually far too long to read; the
        // filename is the part that identifies the picture.
        text: root.source
              ? decodeURIComponent(String(root.source).split("/").pop()) : ""
        color: Theme.textGhost
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeKicker
        elide: Text.ElideMiddle
    }

    // A picture that used to be there and is not any more would otherwise just
    // draw nothing, which reads as "I set it wrong" rather than "it moved".
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        visible: root.source !== "" && thumbnail.status === Image.Error

        AppIcon {
            path: Icons.warning
            size: 13
            color: Theme.warning
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("This file is missing or cannot be read.")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeKicker
            wrapMode: Text.WordWrap
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2

        AppButton {
            Layout.fillWidth: true
            dense: true
            iconPath: Icons.image
            text: root.source ? qsTr("Change…") : qsTr("Choose image…")
            onClicked: imageDialog.open()
        }

        IconButton {
            iconPath: Icons.close
            iconSize: 13
            visible: root.source !== ""
            tip: qsTr("Clear image")
            onClicked: if (root.node) root.node.source = ""
        }
    }

    SegmentedControl {
        label: qsTr("Fill mode"); target: root.node; propertyName: "fillMode"
        info: qsTr("How the image meets its box: Crop fills it and loses the "
                   + "overhang, Fit shows all of it and leaves bars, Stretch "
                   + "distorts it to fit exactly.")
        options: [
            { text: qsTr("Crop"), value: "crop" },
            { text: qsTr("Fit"), value: "fit" },
            { text: qsTr("Stretch"), value: "stretch" }
        ]
    }

    SectionHeader { text: qsTr("Crop") }

    // The whole source with the crop rectangle drawn over it, so the four
    // sliders below are something you aim rather than something you solve.
    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 120
        visible: root.source !== "" && thumbnail.status === Image.Ready

        Image {
            id: thumbnail

            anchors.fill: parent
            source: root.source
            fillMode: Image.PreserveAspectFit
            // A preview, not the render: decoding the full file here would cost
            // as much as the canvas does.
            sourceSize.width: 480
            sourceSize.height: 480
        }

        // Scaled to the thumbnail's PAINTED rect, not its item rect: with
        // PreserveAspectFit the two differ on any non-matching aspect, and the
        // outline would sit off the picture it is describing.
        Rectangle {
            readonly property real fitX: thumbnail.x
                + (thumbnail.width - thumbnail.paintedWidth) / 2
            readonly property real fitY: thumbnail.y
                + (thumbnail.height - thumbnail.paintedHeight) / 2

            color: "transparent"
            border.width: 1
            border.color: Theme.accent

            x: fitX + root.crop("cropX", 0) * thumbnail.paintedWidth
            y: fitY + root.crop("cropY", 0) * thumbnail.paintedHeight
            width: root.crop("cropW", 1) * thumbnail.paintedWidth
            height: root.crop("cropH", 1) * thumbnail.paintedHeight
        }
    }

    ParamSlider {
        label: qsTr("Left")
        info: qsTr("Where the kept region starts across the source image, as a "
                   + "fraction of its width.")
        target: root.node; propertyName: "cropX"
        from: 0; to: 1
    }
    ParamSlider {
        label: qsTr("Top")
        info: qsTr("Where the kept region starts down the source image, as a "
                   + "fraction of its height.")
        target: root.node; propertyName: "cropY"
        from: 0; to: 1
    }
    ParamSlider {
        label: qsTr("Width")
        info: qsTr("How much of the source's width to keep. 1 is the whole "
                   + "picture; a smaller value zooms in.")
        target: root.node; propertyName: "cropW"
        from: 0.01; to: 1
    }
    ParamSlider {
        label: qsTr("Height")
        info: qsTr("How much of the source's height to keep. 1 is the whole "
                   + "picture; a smaller value zooms in.")
        target: root.node; propertyName: "cropH"
        from: 0.01; to: 1
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Choose an image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: if (root.node) root.node.source = selectedFile
    }
}
