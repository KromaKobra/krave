import QtQuick
import QtQuick.Controls.Basic

import krave

// The typed half of a numeric parameter.
//
// Sliders are fast but imprecise; every slider in the panel is paired with one of
// these so an exact value can always be dialled in. The field reformats itself
// from the model whenever it is not being typed into, so a drag and a keystroke
// never fight over the same text.
//
// Every keystroke is applied straight away -- Enter and clicking away only drop
// focus, they are not a commit step -- and the text shows the stored double at
// full precision, so a typed value is never quietly rounded behind your back.
TextField {
    id: control

    property real value: 0
    /// Optional hard limits. Left open by ParamSlider, where the slider range is a
    /// suggestion rather than a cap; set where the consumer genuinely cannot cope
    /// with anything outside them, as the export resolution does.
    property real from: -Infinity
    property real to: Infinity
    /// Rendered to the right of the number: dB, Hz, ms, %.
    property string unit: ""

    /// The resting width, before any allowance for a long value.
    readonly property real baseWidth: 64 + (unit === "" ? 0
                                                        : unitLabel.implicitWidth + Theme.space2)

    /// Emitted whenever the user's text parses, which is on every keystroke.
    signal edited(real newValue)

    function format(v) {
        const n = Number(v)
        if (!isFinite(n))
            return "0"
        // toPrecision(15) drops binary-representation noise (0.1 + 0.2 -> 0.3)
        // while keeping every digit a double can actually distinguish; Number()
        // re-shortens it so 2 prints as "2", not "2.00000000000000".
        return String(Number(n.toPrecision(15)))
    }

    function reformat() {
        text = format(value)
    }

    /// The typed text as a number, or undefined while it is a fragment that cannot
    /// mean anything yet -- "", "-", ".".
    function parsedValue() {
        const n = parseFloat(text)
        return isFinite(n) ? Math.max(from, Math.min(to, n)) : undefined
    }

    function apply() {
        const n = parsedValue()
        if (n !== undefined && n !== value)
            edited(n)
    }

    function commit() {
        apply()
        reformat()
    }

    onValueChanged: if (!activeFocus) reformat()
    Component.onCompleted: reformat()
    // textEdited fires for user input only, never for the assignment in reformat(),
    // so the model and the text cannot chase each other round.
    onTextEdited: apply()
    onAccepted: control.focus = false
    onEditingFinished: commit()
    onActiveFocusChanged: if (!activeFocus) reformat()

    // Grow with the number so a full-precision value stays readable; past the cap
    // the text scrolls to the caret on its own.
    implicitWidth: Math.min(baseWidth + 116,
                            Math.max(baseWidth,
                                     contentWidth + leftPadding + rightPadding + 4))
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
    // Exponents included: at full precision the field can print 1e-7 itself, and
    // whatever it prints has to be typeable back in.
    validator: RegularExpressionValidator {
        regularExpression: /[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?/
    }

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
