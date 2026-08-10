import QtQuick
import QtQuick.Controls.Basic

import MusicVisualizer

// A square, label-less action. Used wherever a row is too tight for words:
// layer visibility, reordering, transport, panel headers.
Button {
    id: control

    property string iconPath: ""
    property bool iconFilled: false
    property real iconSize: 15
    property real diameter: Theme.iconButtonSize
    /// Draws the accent outline used for the primary transport action.
    property bool outlined: false
    /// Marks a toggle as engaged; the icon takes the accent.
    property bool active: false
    /// Rounds the background to a circle instead of the standard radius.
    property bool circular: false
    property color ink: active || outlined ? Theme.accent : Theme.textMuted
    /// Hover description. Empty means no tooltip.
    property alias tip: tooltip.text

    implicitWidth: diameter
    implicitHeight: diameter
    padding: 0
    hoverEnabled: true
    opacity: enabled ? 1 : 0.35

    AppToolTip {
        id: tooltip
        visible: control.hovered && text !== ""
    }

    contentItem: AppIcon {
        path: control.iconPath
        filled: control.iconFilled
        size: control.iconSize
        color: control.ink
        anchors.centerIn: parent
    }

    background: Rectangle {
        radius: control.circular ? height / 2 : Theme.radiusMd
        border.width: control.outlined ? 1 : 0
        border.color: Theme.accent

        color: {
            const tinted = control.outlined || control.active
            if (control.down)
                return tinted ? Theme.accentPressFill : Theme.pressFill
            if (control.hovered)
                return tinted ? Theme.accentHoverFill : Theme.hoverFill
            return "transparent"
        }

        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            visible: control.visualFocus
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }
}
