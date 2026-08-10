import QtQuick

import krave

// A small tinted label: resolutions, frame rates, layer kinds, file names.
Rectangle {
    id: root

    property string text: ""
    /// "accent" (tinted), "neutral" (tinted) or "outline" (accent hairline).
    property string variant: "neutral"

    readonly property bool isOutline: variant === "outline"
    readonly property bool isAccent: variant === "accent"

    implicitWidth: label.implicitWidth + Theme.space3 * 2
    implicitHeight: label.implicitHeight + Theme.space1 * 2
    radius: Theme.radiusMd * 0.75

    color: isOutline ? "transparent"
                     : (isAccent ? Theme.accent800 : Theme.neutral900)
    border.width: isOutline ? 1 : 0
    border.color: Theme.accent

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.isOutline ? Theme.accent
                              : (root.isAccent ? Theme.accent100 : Theme.neutral300)
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeMicro
        font.letterSpacing: 0.2
    }
}
