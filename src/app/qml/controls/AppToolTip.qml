import QtQuick
import QtQuick.Controls.Basic

import MusicVisualizer

// A themed tooltip.
//
// Declared as a child of the control it describes rather than through the
// attached `ToolTip.text` property: the attached one routes to a single shared
// popup that carries the platform's own styling, which on this ground reads as a
// white slab dropped onto the interface.
ToolTip {
    id: control

    delay: 450
    padding: Theme.space2
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? parent.height : 0) + Theme.space1

    contentItem: Text {
        text: control.text
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeMicro
        wrapMode: Text.WordWrap
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.surface
        border.width: 1
        border.color: Theme.dividerStrong
    }
}
