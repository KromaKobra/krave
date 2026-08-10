pragma Singleton

import QtQuick

// One place that knows what each layer type is called and what it looks like.
//
// The layer list, the add menu and the property panel all read from here, so a
// new layer type gets its icon, label and blurb by being added to `all` -- not
// by three separate switch statements drifting apart.
QtObject {
    readonly property var all: [
        {
            typeId: "audio",
            title: qsTr("Audio"),
            label: qsTr("analysis"),
            icon: Icons.waveform,
            hint: qsTr("Turns the track into frequency bands other layers read")
        },
        {
            typeId: "bars",
            title: qsTr("Bars"),
            label: qsTr("spectrum"),
            icon: Icons.bars,
            hint: qsTr("Draws the spectrum an audio layer produces")
        },
        {
            typeId: "text",
            title: qsTr("Text"),
            label: qsTr("text"),
            icon: Icons.textT,
            hint: qsTr("A title, artist name or any static caption")
        },
        {
            typeId: "particles",
            title: qsTr("Particles"),
            label: qsTr("particles"),
            icon: Icons.sparkle,
            hint: qsTr("Emits sparks in time with a frequency band")
        },
        {
            typeId: "background",
            title: qsTr("Background"),
            label: qsTr("background"),
            icon: Icons.image,
            hint: qsTr("A flat colour or a still image behind everything")
        }
    ]

    function entry(typeId) {
        for (let i = 0; i < all.length; ++i) {
            if (all[i].typeId === typeId)
                return all[i]
        }
        return null
    }

    function icon(typeId) {
        const e = entry(typeId)
        return e ? e.icon : Icons.stack
    }

    function label(typeId) {
        const e = entry(typeId)
        return e ? e.label : typeId
    }

    function title(typeId) {
        const e = entry(typeId)
        return e ? e.title : typeId
    }
}
