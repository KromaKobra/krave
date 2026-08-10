import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave

// The one button in the app.
//
// Actions are outlined, never filled: the primary variant is an accent border on
// transparent, and the accent arrives as a tint on hover. Filling a control with
// the accent would make it the loudest thing on screen, which the canvas should
// always be.
Button {
    id: control

    /// "primary" (accent outline), "secondary" (neutral outline) or
    /// "ghost" (no outline, accent ink).
    property string variant: "secondary"
    /// Path data from Icons, drawn ahead of the label.
    property string iconPath: ""
    property bool iconFilled: false
    property real iconSize: 14
    /// Compact rows use this to drop to the small control height.
    property bool dense: false

    readonly property bool isPrimary: variant === "primary"
    readonly property bool isGhost: variant === "ghost"
    readonly property color ink: (isPrimary || isGhost) ? Theme.accent : Theme.text

    implicitHeight: dense ? Theme.controlHeightSm : Theme.controlHeight
    leftPadding: isGhost ? Theme.space2 : Theme.space4
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    opacity: enabled ? 1 : 0.45

    contentItem: RowLayout {
        spacing: Theme.space2

        AppIcon {
            visible: control.iconPath !== ""
            path: control.iconPath
            filled: control.iconFilled
            size: control.iconSize
            color: control.ink
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            visible: control.text !== ""
            text: control.text
            color: control.ink
            font.family: Theme.fontFamily
            font.pixelSize: control.dense ? Theme.fontSizeSmall : Theme.fontSizeBody
            font.weight: Theme.weightMedium
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        border.width: control.isGhost ? 0 : 1
        border.color: control.isPrimary ? Theme.accent : Theme.divider

        color: {
            const tinted = control.isPrimary || control.isGhost
            if (control.down)
                return tinted ? Theme.accentPressFill : Theme.pressFill
            if (control.hovered)
                return tinted ? Theme.accentHoverFill : Theme.hoverFill
            return "transparent"
        }

        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        // Keyboard focus is the accent ring; the platform's own is never used.
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
