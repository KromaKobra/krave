import QtQuick
import QtQuick.Controls.Basic

import krave

// A themed tooltip.
//
// Declared as a child of the control it describes rather than through the
// attached `ToolTip.text` property: the attached one routes to a single shared
// popup that carries the platform's own styling, which on this ground reads as a
// white slab dropped onto the interface.
ToolTip {
    id: control

    /// Where a sentence starts wrapping instead of running off sideways.
    property real maxWidth: 260

    /// How far the tip has to slide back to stay inside the window. Centring it
    /// under a control near the right edge of a panel would otherwise push it off.
    readonly property real edgeShift: {
        if (!parent || !Overlay.overlay)
            return 0
        const centred = parent.mapToItem(Overlay.overlay, (parent.width - width) / 2, 0).x
        const margin = Theme.space2
        const overflow = centred + width - (Overlay.overlay.width - margin)
        if (overflow > 0)
            return -overflow
        return centred < margin ? margin - centred : 0
    }

    delay: 450
    padding: Theme.space2
    x: (parent ? (parent.width - width) / 2 : 0) + edgeShift
    y: (parent ? parent.height : 0) + Theme.space1

    // Measured rather than taken from the Text: a wrapping Text reports the width
    // it would need unwrapped, so feeding that back as the container's width is a
    // binding loop waiting to happen.
    implicitWidth: Math.min(maxWidth, metrics.advanceWidth + leftPadding + rightPadding + 1)

    TextMetrics {
        id: metrics
        font: tipText.font
        text: control.text
    }

    contentItem: Text {
        id: tipText

        width: control.availableWidth
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
