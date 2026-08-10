import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave

// A boolean setting: name on the left, switch on the right.
//
// Binds to a target property when given one, and otherwise works as a plain
// checked/toggled control for state that lives in QML.
RowLayout {
    id: root

    property string label: ""
    /// One-line explanation, revealed by the info icon beside the name.
    property string info: ""
    /// Long-form explanation, opened by clicking that icon. Optional.
    property string details: ""
    property QtObject target: null
    property string propertyName: ""
    /// Used when no target is set.
    property bool checked: false
    /// Indents the row so it reads as a detail of the control above it.
    property bool sub: false

    readonly property bool value: {
        if (!target || !propertyName)
            return checked
        const v = target[propertyName]
        return v === undefined ? false : v
    }

    signal toggled(bool newValue)

    function flip() {
        const next = !value
        if (target && propertyName)
            target[propertyName] = next
        else
            checked = next
        toggled(next)
    }

    Layout.fillWidth: true
    Layout.leftMargin: root.sub ? Theme.space4 : 0
    spacing: Theme.space3

    ParamLabel {
        text: root.label
        info: root.info
        details: root.details
    }

    Rectangle {
        id: track

        Layout.alignment: Qt.AlignVCenter
        implicitWidth: 34
        implicitHeight: 18
        radius: height / 2
        color: root.value ? Theme.accentSoftFill : "transparent"
        border.width: 1
        border.color: root.value ? Theme.accent : Theme.dividerStrong

        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            width: 12
            height: 12
            radius: 6
            y: (parent.height - height) / 2
            x: root.value ? parent.width - width - 3 : 3
            color: root.value ? Theme.accent : Theme.neutral500

            Behavior on x { NumberAnimation { duration: Theme.durNormal; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: Theme.durFast } }
        }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.flip() }
    }
}
