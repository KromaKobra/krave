import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Slider plus a numeric readout, bound two-way to a target object property.
//
// Writes go straight to the C++ Q_PROPERTY, which is what makes edits show up on
// the canvas immediately.
ColumnLayout {
    id: root

    property string label: ""
    property QtObject target: null
    property string propertyName: ""
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property int decimals: 2

    // Sections for other layer types stay instantiated while hidden, so this can
    // legitimately be pointed at a layer that has no such property. Fall back to
    // 0 instead of letting `undefined` reach the slider.
    readonly property real value: {
        if (!target || !propertyName)
            return 0
        const v = target[propertyName]
        return v === undefined ? 0 : v
    }

    spacing: 2
    enabled: target !== null

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: root.label
            color: "#c8c8d4"
            font.pixelSize: 11
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Text {
            text: root.value.toFixed(root.decimals)
            color: "#8a8a9a"
            font.pixelSize: 11
            font.family: "Consolas"
        }
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.from
        to: root.to
        stepSize: root.stepSize
        // Track the model, but do not fight the user while they drag.
        value: root.value
        onMoved: {
            if (root.target && root.propertyName)
                root.target[root.propertyName] = value
        }

        implicitHeight: 20

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 3
            radius: 2
            color: "#2a2a38"

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                color: "#4cc9f0"
                radius: 2
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 12
            height: 12
            radius: 6
            color: slider.pressed ? "#ffffff" : "#e0e0ea"
            border.color: "#4cc9f0"
            border.width: 1
        }
    }
}
