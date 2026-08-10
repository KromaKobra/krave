import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import MusicVisualizer

// A dropdown bound to a string-valued property on a backend object. Used where
// the options are too many or too wordy for a segmented control.
ColumnLayout {
    id: root

    property string label: ""
    property QtObject target: null
    property string propertyName: ""
    /// List of { text, value } entries.
    property var options: []
    /// Selection when no target is bound. Bind it to whatever the caller stores
    /// and update that from `activated` -- this control never writes it back, so
    /// the binding survives.
    property var localValue: undefined

    readonly property bool bound: target !== null && propertyName !== ""
    readonly property var currentValue: bound ? target[propertyName] : localValue

    signal activated(var value)

    function choose(value) {
        if (bound)
            target[propertyName] = value
        activated(value)
    }

    Layout.fillWidth: true
    spacing: Theme.space2
    enabled: propertyName === "" || target !== null

    Text {
        visible: root.label !== ""
        text: root.label
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }

    ComboBox {
        id: combo

        Layout.fillWidth: true
        implicitHeight: Theme.controlHeight
        model: root.options
        textRole: "text"
        valueRole: "value"

        currentIndex: {
            const current = root.currentValue
            for (let i = 0; i < root.options.length; ++i) {
                if (root.options[i].value === current)
                    return i
            }
            return -1
        }

        onActivated: (index) => root.choose(root.options[index].value)

        contentItem: Text {
            leftPadding: Theme.space3
            rightPadding: Theme.space8
            text: combo.displayText
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: AppIcon {
            x: combo.width - width - Theme.space3
            y: (combo.height - height) / 2
            path: Icons.chevronDown
            size: 14
            color: Theme.neutral500
            rotation: combo.popup.visible ? 180 : 0

            Behavior on rotation { NumberAnimation { duration: Theme.durNormal } }
        }

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.sunken
            border.width: 1
            border.color: (combo.activeFocus || combo.popup.visible)
                          ? Theme.accent
                          : (combo.hovered ? Theme.dividerStrong : Theme.divider)

            Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
        }

        popup: Popup {
            y: combo.height + Theme.space1
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + Theme.space2 * 2, 280)
            padding: Theme.space1

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.surface
                border.width: 1
                border.color: Theme.dividerStrong
            }

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar {}
            }
        }

        delegate: ItemDelegate {
            id: option

            required property int index
            required property var modelData

            width: ListView.view ? ListView.view.width : 0
            implicitHeight: Theme.controlHeight
            padding: Theme.space2
            highlighted: combo.highlightedIndex === index

            contentItem: RowLayout {
                spacing: Theme.space2

                Text {
                    Layout.fillWidth: true
                    text: option.modelData.text
                    color: combo.currentIndex === option.index ? Theme.accent : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideRight
                }

                AppIcon {
                    visible: combo.currentIndex === option.index
                    path: Icons.check
                    size: 13
                    color: Theme.accent
                }
            }

            background: Rectangle {
                radius: Theme.radiusSm
                color: option.highlighted ? Theme.hoverFill : "transparent"
            }
        }
    }
}
