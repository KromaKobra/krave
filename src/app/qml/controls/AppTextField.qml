import QtQuick
import QtQuick.Controls.Basic

import krave

// A single-line text field on the sunken well, with the accent reserved for the
// caret and the focused border.
TextField {
    id: control

    property string placeholder: ""

    implicitHeight: Theme.controlHeight
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    topPadding: 0
    bottomPadding: 0

    color: Theme.text
    placeholderText: placeholder
    placeholderTextColor: Theme.textGhost
    selectionColor: Theme.accentSoftFill
    selectedTextColor: Theme.text
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    // Callers apply on every keystroke, so Enter has nothing left to commit and
    // just steps out of the field.
    onAccepted: control.focus = false

    cursorDelegate: Rectangle {
        width: 1
        color: Theme.accent
        visible: control.activeFocus

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: control.activeFocus
            NumberAnimation { to: 0; duration: 500; easing.type: Easing.OutQuad }
            NumberAnimation { to: 1; duration: 500; easing.type: Easing.InQuad }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.sunken
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                                          : (control.hovered ? Theme.dividerStrong : Theme.divider)

        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }
}
