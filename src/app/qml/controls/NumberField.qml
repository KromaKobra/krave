import QtQuick
import QtQuick.Controls.Basic

import MusicVisualizer

// The typed half of a numeric parameter.
//
// Sliders are fast but imprecise; every slider in the panel is paired with one of
// these so an exact value can always be dialled in. The field reformats itself
// from the model whenever it is not being typed into, so a drag and a keystroke
// never fight over the same text.
TextField {
    id: control

    property real value: 0
    property int decimals: 2
    property real from: -Infinity
    property real to: Infinity
    /// Rendered to the right of the number: dB, Hz, ms, %.
    property string unit: ""

    /// Emitted once the user commits a parseable value.
    signal edited(real newValue)

    function reformat() {
        text = Number(value).toFixed(decimals)
    }

    function commit() {
        const parsed = parseFloat(text)
        if (isFinite(parsed)) {
            const clamped = Math.max(from, Math.min(to, parsed))
            if (clamped !== value)
                edited(clamped)
        }
        reformat()
    }

    onValueChanged: if (!activeFocus) reformat()
    Component.onCompleted: reformat()
    onEditingFinished: commit()
    onActiveFocusChanged: if (!activeFocus) reformat()

    implicitWidth: 64 + (unit === "" ? 0 : unitLabel.implicitWidth + Theme.space2)
    implicitHeight: Theme.controlHeightSm
    leftPadding: Theme.space2
    rightPadding: Theme.space2 + (unit === "" ? 0 : unitLabel.implicitWidth + Theme.space1)
    topPadding: 0
    bottomPadding: 0

    color: Theme.text
    font.family: Theme.monoFamily
    font.pixelSize: Theme.fontSizeSmall
    horizontalAlignment: TextInput.AlignRight
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    validator: RegularExpressionValidator { regularExpression: /-?[0-9]*\.?[0-9]*/ }

    cursorDelegate: Rectangle {
        width: 1
        color: Theme.accent
        visible: control.activeFocus
    }

    Text {
        id: unitLabel
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        visible: control.unit !== ""
        text: control.unit
        color: Theme.textGhost
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeKicker
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.sunken
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                                          : (control.hovered ? Theme.dividerStrong : Theme.divider)

        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }
}
