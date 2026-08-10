import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave

// A setting's name, with its explanation hidden behind an info icon.
//
// The icon stays invisible until the pointer is over the name, so a panel of
// sixty settings reads as sixty names rather than sixty names and sixty icons.
// It occupies its space the whole time regardless, because a row that reflows
// under the cursor is worse than one that shows a little more ink.
RowLayout {
    id: root

    property string text: ""
    /// One line, shown when the icon is hovered. No info means no icon at all.
    property string info: ""
    /// The long version, opened by clicking the icon. Optional: most settings are
    /// adequately covered by their one-liner, and an icon with nothing behind it
    /// does not offer to be clicked.
    property string details: ""

    readonly property bool hasIcon: info !== "" || details !== ""
    readonly property bool detailsOpen: detailLoader.active
    readonly property real iconSize: 12

    /// Opens the detail window, building it first. Does nothing on a setting whose
    /// one-liner is the whole story.
    function openDetails() {
        if (details === "")
            return
        detailLoader.active = true
        detailLoader.item.open()
    }

    Layout.fillWidth: true
    spacing: 0

    // The hover target: the name and the icon, nothing beyond them. Filling the
    // row would arm the icon from halfway across the panel; the maximum keeps the
    // rectangle on the words while still letting a long name elide.
    Item {
        id: region

        Layout.fillWidth: true
        Layout.maximumWidth: label.implicitWidth
                             + (root.hasIcon ? root.iconSize + Theme.space1 : 0)
        implicitHeight: Math.max(label.implicitHeight, root.iconSize)

        HoverHandler { id: regionHover }

        Text {
            id: label

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - (root.hasIcon ? root.iconSize + Theme.space1 : 0)
            text: root.text
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }

        AppIcon {
            id: icon

            anchors.left: label.right
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            visible: root.hasIcon
            path: Icons.info
            size: root.iconSize
            weight: 1.7
            color: iconHover.hovered ? Theme.textMuted : Theme.textGhost
            opacity: regionHover.hovered ? 1 : 0

            Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
            Behavior on color { ColorAnimation { duration: Theme.durFast } }

            HoverHandler {
                id: iconHover
                cursorShape: root.details !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
            }

            TapHandler {
                enabled: root.details !== ""
                onTapped: root.openDetails()
            }

            AppToolTip {
                text: root.info
                visible: iconHover.hovered && root.info !== ""
            }
        }
    }

    // The slack. A layout hands its own maximum width up from its children, so
    // without something unbounded here the capped hover region would cap the whole
    // label, and whatever sits beside it -- a number field, a switch -- would be
    // dragged in from the right edge to meet the text.
    Item { Layout.fillWidth: true }

    // Built on the first click and dropped again on close: sixty of these standing
    // by permanently would cost more than the rare one costs to construct.
    Loader {
        id: detailLoader
        active: false
        sourceComponent: detailDialog
    }

    Component {
        id: detailDialog

        AppDialog {
            id: dialog

            title: root.text
            width: 420
            onClosed: detailLoader.active = false

            contentItem: Text {
                text: root.details
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                lineHeight: 1.4
                wrapMode: Text.WordWrap
            }

            footer: RowLayout {
                spacing: Theme.space2

                Item { Layout.fillWidth: true; Layout.preferredHeight: 56 }

                AppButton {
                    Layout.rightMargin: Theme.space6
                    text: qsTr("Close")
                    variant: "ghost"
                    onClicked: dialog.close()
                }
            }
        }
    }
}
