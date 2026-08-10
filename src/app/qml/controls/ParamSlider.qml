import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave

// A numeric layer parameter: name on the left, exact value on the right, slider
// underneath.
//
// Both halves write straight to the C++ Q_PROPERTY, and the canvas binds to that
// same property -- which is why an edit shows up on the very next frame with no
// apply step anywhere in between.
ColumnLayout {
    id: root

    property string label: ""
    /// One-line explanation, revealed by the info icon beside the name.
    property string info: ""
    /// Long-form explanation, opened by clicking that icon. Optional.
    property string details: ""
    property QtObject target: null
    property string propertyName: ""
    /// The slider's range. A suggestion rather than a cap: a typed value may sit
    /// outside it, and the handle simply pins at whichever end it ran past.
    property real from: 0
    property real to: 1
    /// The granularity of a drag, an arrow key and a wheel notch alike.
    property real stepSize: 0.01
    /// Where the slider's fill starts; 0 on a signed parameter puts it mid-track.
    property real origin: from
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

    /// Land a dragged value on the step grid. The number field shows what is stored
    /// verbatim, so a drag has to produce a number worth reading: toPrecision(15)
    /// clears the float noise the multiply leaves behind, making 7 steps of 0.01
    /// read as 0.07 rather than 0.07000000000000001. Typed values skip this and
    /// keep every digit they were given.
    function snap(v) {
        if (stepSize <= 0)
            return v
        return Number((from + Math.round((v - from) / stepSize) * stepSize).toPrecision(15))
    }

    Layout.fillWidth: true
    spacing: Theme.space1
    enabled: target !== null

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2

        ParamLabel {
            text: root.label
            info: root.info
            details: root.details
        }

        NumberField {
            value: root.value
            unit: root.unit
            onEdited: (v) => root.apply(v)
        }
    }

    AppSlider {
        Layout.fillWidth: true
        from: root.from
        to: root.to
        origin: root.origin
        stepSize: root.stepSize
        // Track the model, but do not fight the user while they drag. A typed value
        // beyond the range is clamped here for display only; root.value keeps it.
        value: root.value
        onMoved: root.apply(root.snap(value))
    }
}
