// Content and typography for a text layer.
import QtQuick
import QtQuick.Controls.Basic
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
    visible: root.nodeType === "text"

    SectionHeader { text: qsTr("Content") }

    AppTextField {
        Layout.fillWidth: true
        text: (root.node && root.node.text !== undefined) ? root.node.text : ""
        placeholder: qsTr("Track title")
        onTextEdited: if (root.node) root.node.text = text
    }

    AppButton {
        Layout.fillWidth: true
        text: qsTr("Use track title")
        iconPath: Icons.link
        dense: true
        enabled: audioEngine.hasTrack && root.node !== null
        onClicked: root.node.text = audioEngine.trackTitle
    }

    SectionHeader { text: qsTr("Type") }

    ParamSlider {
        label: qsTr("Font size")
        info: qsTr("Text size as a fraction of canvas height, so it scales "
                   + "with the export resolution instead of shrinking.")
        target: root.node; propertyName: "fontScale"
        from: 0.01; to: 0.3; stepSize: 0.001
    }
    SegmentedControl {
        label: qsTr("Alignment"); target: root.node; propertyName: "alignment"
        info: qsTr("How the text sits within the layer's box.")
        options: [
            { text: qsTr("Left"), value: "left" },
            { text: qsTr("Centre"), value: "center" },
            { text: qsTr("Right"), value: "right" }
        ]
    }
    ToggleRow {
        label: qsTr("Bold")
        info: qsTr("Draws the text in the font's bold weight.")
        target: root.node
        propertyName: "bold"
    }
    ColorField {
        label: qsTr("Colour"); target: root.node; propertyName: "color"
        info: qsTr("The colour the text is drawn in.")
    }
}
