import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import MusicVisualizer

// A numeric layer parameter: name on the left, exact value on the right, slider
// underneath.
//
// Both halves write straight to the C++ Q_PROPERTY, and the canvas binds to that
// same property -- which is why an edit shows up on the very next frame with no
// apply step anywhere in between.
ColumnLayout {
    id: root

    property string label: ""
    property QtObject target: null
    property string propertyName: ""
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property int decimals: 2
    /// Suffix shown inside the number field: dB, Hz, ms, %.
    property string unit: ""

    // Sections for the other layer types stay instantiated while hidden, so this
    // can legitimately be pointed at a layer that has no such property. Fall back
    // to 0 rather than letting `undefined` reach the slider.
    readonly property real value: {
        if (!target || !propertyName)
            return 0
        const v = target[propertyName]
        return v === undefined ? 0 : v
    }

    function apply(v) {
        if (target && propertyName)
            target[propertyName] = v
    }

    Layout.fillWidth: true
    spacing: Theme.space1
    enabled: target !== null

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2

        Text {
            Layout.fillWidth: true
            text: root.label
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }

        NumberField {
            value: root.value
            decimals: root.decimals
            from: root.from
            to: root.to
            unit: root.unit
            onEdited: (v) => root.apply(v)
        }
    }

    AppSlider {
        Layout.fillWidth: true
        from: root.from
        to: root.to
        stepSize: root.stepSize
        // Track the model, but do not fight the user while they drag.
        value: root.value
        onMoved: root.apply(value)
    }
}
