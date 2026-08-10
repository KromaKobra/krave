import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import MusicVisualizer

// A choice between two or three short options, shown all at once.
//
// Preferred over a dropdown wherever the options are few and their labels are
// short: the current setting and its alternatives are both readable without a
// click, which matters in a panel the user is sweeping through.
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

    Rectangle {
        Layout.fillWidth: true
        implicitWidth: segments.implicitWidth
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusMd
        color: "transparent"
        border.width: 1
        border.color: Theme.divider

        RowLayout {
            id: segments
            anchors.fill: parent
            spacing: 0

            Repeater {
                model: root.options

                delegate: Item {
                    id: segment

                    required property int index
                    required property var modelData

                    readonly property bool checked: root.currentValue === modelData.value
                    // Only the outer corners round; the seams between segments
                    // stay square so the fills meet flush.
                    readonly property real outerRadius: Theme.radiusMd
                    readonly property bool first: index === 0
                    readonly property bool last: index === root.options.length - 1

                    implicitWidth: segmentLabel.implicitWidth + Theme.space4 * 2
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: implicitWidth

                    Rectangle {
                        anchors.fill: parent
                        color: hover.hovered && !segment.checked ? Theme.hoverFill : "transparent"
                        topLeftRadius: segment.first ? segment.outerRadius : 0
                        bottomLeftRadius: segment.first ? segment.outerRadius : 0
                        topRightRadius: segment.last ? segment.outerRadius : 0
                        bottomRightRadius: segment.last ? segment.outerRadius : 0

                        Behavior on color { ColorAnimation { duration: Theme.durFast } }
                    }

                    // Selection is an accent hairline, never a flood -- the same
                    // move the primary button makes.
                    Rectangle {
                        anchors.fill: parent
                        visible: segment.checked
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.accent
                        topLeftRadius: segment.first ? segment.outerRadius : 0
                        bottomLeftRadius: segment.first ? segment.outerRadius : 0
                        topRightRadius: segment.last ? segment.outerRadius : 0
                        bottomRightRadius: segment.last ? segment.outerRadius : 0
                    }

                    Rectangle {
                        visible: !segment.first && !segment.checked
                        width: 1
                        color: Theme.divider
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 1
                        anchors.bottomMargin: 1
                    }

                    Text {
                        id: segmentLabel
                        anchors.centerIn: parent
                        width: parent.width - Theme.space2 * 2
                        text: segment.modelData.text
                        color: segment.checked ? Theme.accent : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    HoverHandler {
                        id: hover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: root.choose(segment.modelData.value)
                    }
                }
            }
        }
    }
}
