pragma Singleton

import QtQuick

// One place that knows what each node type is called and what it looks like.
//
// PRESENTATION ONLY. Which ports a type has, what they accept, whether it can be
// created and whether it draws all live in the C++ port table (NodePorts.h) --
// because the flatten, cycle detection, the deletion cascade and link validation
// all read that and all have to run headless in the tests.
//
// So the add menu's model is nodeGraph.creatableTypes(), and this file is what
// turns a typeId from it into an icon, a title and a blurb.
QtObject {
    readonly property var all: [
        {
            typeId: "output",
            title: qsTr("Output"),
            label: qsTr("output"),
            icon: Icons.output,
            hint: qsTr("What gets rendered: whatever reaches this node's input")
        },
        {
            typeId: "audio",
            title: qsTr("Audio"),
            label: qsTr("source"),
            icon: Icons.waveform,
            hint: qsTr("The track itself, as one band covering the whole spectrum")
        },
        {
            typeId: "separator",
            title: qsTr("Separator"),
            label: qsTr("analysis"),
            icon: Icons.separator,
            hint: qsTr("Splits the audio it is given into frequency bands")
        },
        {
            typeId: "spectrum",
            title: qsTr("Spectrum"),
            label: qsTr("spectrum"),
            icon: Icons.bars,
            hint: qsTr("Draws the bands a separator produces")
        },
        {
            typeId: "text",
            title: qsTr("Text"),
            label: qsTr("text"),
            icon: Icons.textT,
            hint: qsTr("A title, artist name or any static caption")
        },
        {
            typeId: "background",
            title: qsTr("Background"),
            label: qsTr("background"),
            icon: Icons.image,
            hint: qsTr("A flat colour behind everything")
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
        return e ? e.icon : Icons.nodes
    }

    function label(typeId) {
        const e = entry(typeId)
        return e ? e.label : typeId
    }

    function title(typeId) {
        const e = entry(typeId)
        return e ? e.title : typeId
    }

    function hint(typeId) {
        const e = entry(typeId)
        return e ? e.hint : ""
    }
}
