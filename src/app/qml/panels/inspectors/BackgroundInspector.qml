// Fill colour for a background node.
//
// Colour only: a still image is an Image node's job, not a background's, and
// merging the two was what made "is there a picture behind this colour?" a
// question you had to open the panel to answer.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../../controls"

ColumnLayout {
    id: root

    /// The selected Node, or null.
    property var node: null
    /// Its typeId, hoisted so the visibility test costs no null check.
    property string nodeType: ""

    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "background"

    SectionHeader { text: qsTr("Fill") }

    ColorField {
        label: qsTr("Colour"); target: root.node; propertyName: "color"
        info: qsTr("The flat colour behind everything else.")
    }
}
