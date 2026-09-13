import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave

// A quiet one-line warning strip across the top of the window.
//
// For things the user should know about but did not ask about -- a session file
// that named a track which has since been deleted. A modal dialog on every launch
// would be out of proportion to that; this states it, stays out of the way, and
// goes away when dismissed.
//
// Collapses to nothing when there is no message, so a normal launch is unaffected.
Rectangle {
    id: root

    property string text: ""
    property bool dismissed: false

    visible: text !== "" && !dismissed
    implicitHeight: visible ? row.implicitHeight + Theme.space2 * 2 : 0
    color: Theme.withAlpha(Theme.warning, 0.1)

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space3
        spacing: Theme.space2

        AppIcon {
            Layout.alignment: Qt.AlignVCenter
            path: Icons.warning
            size: 13
            color: Theme.warning
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.text
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            iconPath: Icons.close
            iconSize: 12
            diameter: 20
            ink: Theme.warning
            tip: qsTr("Dismiss")
            onClicked: root.dismissed = true
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.divider
    }
}
