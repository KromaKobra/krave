import QtQuick
import QtQuick.Layouts

import krave

// Divides the property panel into named groups.
//
// Every section but the first is preceded by a rule, so the groups read as bands
// rather than as a single undifferentiated column of controls.
ColumnLayout {
    id: root

    property string text: ""
    /// The first header in a panel gets no rule above it.
    property bool leading: false

    Layout.fillWidth: true
    Layout.topMargin: leading ? Theme.space3 : Theme.space6
    spacing: 0

    Rectangle {
        visible: !root.leading
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.space4
        implicitHeight: 1
        color: Theme.divider
    }

    Text {
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.space2
        text: root.text.toUpperCase()
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeKicker
        font.weight: Theme.weightMedium
        font.letterSpacing: 0.9
    }
}
