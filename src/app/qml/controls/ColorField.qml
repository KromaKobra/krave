import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

import krave

// A colour bound to a QColor property.
//
// The swatch opens the system picker; the hex beside it is editable, because
// matching a colour to a brand or an album cover is usually a paste, not a hunt
// through a colour wheel.
ColumnLayout {
    id: root

    property string label: ""
    /// One-line explanation, revealed by the info icon beside the name.
    property string info: ""
    /// Long-form explanation, opened by clicking that icon. Optional.
    property string details: ""
    property QtObject target: null
    property string propertyName: ""

    readonly property color value: {
        if (!target || !propertyName)
            return "#000000"
        const v = target[propertyName]
        return v === undefined ? "#000000" : v
    }

    function apply(c) {
        if (target && propertyName)
            target[propertyName] = c
    }

    Layout.fillWidth: true
    spacing: Theme.space2
    enabled: target !== null

    ParamLabel {
        visible: root.label !== ""
        text: root.label
        info: root.info
        details: root.details
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusMd
        color: Theme.sunken
        border.width: 1
        border.color: hex.activeFocus ? Theme.accent
                                      : (swatchHover.hovered ? Theme.dividerStrong : Theme.divider)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            spacing: Theme.space2

            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 5
                color: root.value
                border.width: 1
                border.color: Theme.withAlpha(Theme.text, 0.18)

                HoverHandler {
                    id: swatchHover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler { onTapped: dialog.open() }
            }

            TextField {
                id: hex

                Layout.fillWidth: true
                implicitHeight: parent.height
                padding: 0
                background: null
                color: Theme.text
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeSmall
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
                selectionColor: Theme.accentSoftFill
                selectedTextColor: Theme.text

                // Reformat from the model except while it is being typed into, so
                // a picker drag and a keystroke never fight over the same text.
                function reformat() {
                    text = root.value.toString().toUpperCase()
                }

                /// Applies as soon as the text is a whole colour, which keeps the
                /// canvas live without painting it with half-typed hex.
                function apply() {
                    const hexText = text.trim()
                    if (/^#?[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/.test(hexText))
                        root.apply(hexText.charAt(0) === "#" ? hexText : "#" + hexText)
                }

                Component.onCompleted: reformat()
                onActiveFocusChanged: if (!activeFocus) reformat()
                onTextEdited: apply()
                onAccepted: hex.focus = false
                onEditingFinished: { apply(); reformat() }

                Connections {
                    target: root
                    function onValueChanged() {
                        if (!hex.activeFocus)
                            hex.reformat()
                    }
                }

                cursorDelegate: Rectangle {
                    width: 1
                    color: Theme.accent
                    visible: hex.activeFocus
                }
            }

            IconButton {
                iconPath: Icons.sliders
                iconSize: 13
                diameter: 22
                tip: qsTr("Pick a colour…")
                onClicked: dialog.open()
            }
        }
    }

    ColorDialog {
        id: dialog
        selectedColor: root.value
        onAccepted: root.apply(selectedColor)
    }
}
