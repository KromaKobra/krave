import QtQuick
import QtQuick.Controls.Basic

import MusicVisualizer

// The themed slider: a hairline track, an accent fill and a handle that carries
// the accent as a glow rather than growing or changing colour under the cursor.
Slider {
    id: control

    property real trackHeight: 4
    property real handleSize: 13

    implicitHeight: 20

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: control.trackHeight
        radius: height / 2
        color: Theme.neutral800

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.handleSize
        height: control.handleSize
        radius: width / 2
        color: Theme.accent

        Rectangle {
            readonly property real halo: control.pressed ? 12 : (control.hovered ? 10 : 6)

            anchors.centerIn: parent
            width: parent.width + halo
            height: parent.height + halo
            radius: width / 2
            z: -1
            color: Theme.withAlpha(Theme.accent, control.pressed ? 0.3 : 0.22)

            Behavior on width { NumberAnimation { duration: Theme.durFast } }
            Behavior on height { NumberAnimation { duration: Theme.durFast } }
        }
    }
}
