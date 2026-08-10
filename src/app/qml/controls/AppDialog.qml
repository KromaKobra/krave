import QtQuick
import QtQuick.Controls.Basic

import MusicVisualizer

// Shared modal chrome: the top elevation in the system, which on a dark ground
// means a hairline edge and ambient darkness rather than a stack of shadows.
Dialog {
    id: control

    anchors.centerIn: Overlay.overlay
    modal: true
    padding: Theme.space6

    header: Item {
        implicitHeight: control.title === "" ? 0 : 52
        visible: control.title !== ""

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space6
            anchors.verticalCenter: parent.verticalCenter
            text: control.title
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Theme.weightMedium
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.divider
        }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.dividerStrong
    }

    Overlay.modal: Rectangle {
        color: Theme.scrim
    }
}
