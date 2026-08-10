import QtQuick
import QtQuick.Controls.Basic

import krave

// A scrollbar that stays out of the way: a thin neutral thumb on no track, which
// thickens while it is being used. Works in either orientation, so the canvas
// stage can use it on both axes at 100% zoom.
ScrollBar {
    id: control

    // `vertical` is ScrollBar's own read-only property; only the thumb weight is
    // new here.
    readonly property real thickness: (pressed || hovered) ? 6 : 4

    padding: 2
    policy: ScrollBar.AsNeeded

    contentItem: Rectangle {
        implicitWidth: control.vertical ? control.thickness : 0
        implicitHeight: control.vertical ? 0 : control.thickness
        radius: (control.vertical ? width : height) / 2
        color: control.pressed ? Theme.neutral600 : Theme.neutral700
        opacity: control.policy === ScrollBar.AlwaysOn || control.active ? 1 : 0

        Behavior on implicitWidth { NumberAnimation { duration: Theme.durFast } }
        Behavior on implicitHeight { NumberAnimation { duration: Theme.durFast } }
        Behavior on opacity { NumberAnimation { duration: Theme.durNormal } }
    }

    background: null
}
