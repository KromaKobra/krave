// The three effects a blur node carries, and the circle that limits them.
//
// Like TransformInspector, this is here rather than in PropertyPanel's shared
// sections because a blur is not a VisualNode: it has no box of its own, so no
// placement section applies to it.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import krave
import "../../controls"

ColumnLayout {
    id: root

    /// The selected Node, or null.
    property var node: null
    /// Its typeId, hoisted so the visibility test costs no null check.
    property string nodeType: ""

    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "blur"

    SectionHeader { text: qsTr("Blur") }

    ToggleRow {
        label: qsTr("Gaussian")
        info: qsTr("Softens everything feeding this node.")
        details: qsTr("Each layer upstream is blurred on its OWN, not as a "
                      + "composited group. Where two upstream layers overlap "
                      + "that is not the same picture — you get the blur of "
                      + "each, stacked, rather than the blur of the pair. A "
                      + "flat Background upstream also stays looking sharp, "
                      + "because blurring a solid colour returns the same "
                      + "solid colour.\n\n"
                      + "To smear two layers together, put a blur on each arm "
                      + "before the Merge rather than one after it.")
        target: root.node
        propertyName: "gaussianEnabled"
    }
    ParamSlider {
        visible: root.node ? root.node.gaussianEnabled === true : false
        label: qsTr("Radius")
        info: qsTr("How far the blur reaches, as a fraction of the canvas "
                   + "height.")
        details: qsTr("A fraction rather than a pixel count, so the exported "
                      + "frame matches the preview: the same project blurred "
                      + "by 0.02 covers the same share of the picture at "
                      + "640×360 and at 4K.")
        target: root.node; propertyName: "gaussianAmount"
        from: 0; to: 0.2; stepSize: 0.001
    }

    ToggleRow {
        label: qsTr("Chroma blur")
        info: qsTr("Blurs the colour but not the brightness, so edges stay "
                   + "crisp while the colour bleeds across them.")
        details: qsTr("The picture is split into brightness and two colour "
                      + "axes; only the colour axes are softened. It is what "
                      + "cheap video compression does, and at a large radius "
                      + "it reads as colour smeared loose from the shapes "
                      + "carrying it.\n\n"
                      + "Independent of Gaussian: with Gaussian off the "
                      + "brightness stays completely sharp.")
        target: root.node
        propertyName: "chromaBlurEnabled"
    }
    ParamSlider {
        visible: root.node ? root.node.chromaBlurEnabled === true : false
        label: qsTr("Chroma radius")
        info: qsTr("How far the colour bleeds, as a fraction of the canvas "
                   + "height. Usually worth more than the Gaussian radius.")
        target: root.node; propertyName: "chromaBlurAmount"
        from: 0; to: 0.3; stepSize: 0.001
    }

    SectionHeader { text: qsTr("Separation") }

    ToggleRow {
        label: qsTr("Chromatic separation")
        info: qsTr("Pulls red and blue apart along the radius, the way a cheap "
                   + "lens fringes toward the edges of the frame.")
        details: qsTr("The split runs outward from the circle's centre when "
                      + "the circle is on, and from the centre of the canvas "
                      + "when it is not — so the fringe grows with the "
                      + "distance from that point rather than sitting evenly "
                      + "across the picture.")
        target: root.node
        propertyName: "chromaSplitEnabled"
    }
    ParamSlider {
        visible: root.node ? root.node.chromaSplitEnabled === true : false
        label: qsTr("Separation")
        info: qsTr("How far apart the red and blue samples are pulled, as a "
                   + "fraction of the canvas height. Small values do a lot.")
        target: root.node; propertyName: "chromaSplitAmount"
        from: 0; to: 0.05; stepSize: 0.001
    }

    SectionHeader { text: qsTr("Circle") }

    ToggleRow {
        label: qsTr("Limit to circle")
        info: qsTr("Confines all three effects to a circle, instead of "
                   + "applying them evenly across the frame.")
        details: qsTr("The circle scales how much effect each pixel gets, from "
                      + "full at the centre to none outside the soft edge. "
                      + "With Invert on it is the other way round, which is "
                      + "how you get a sharp subject in the middle and a "
                      + "smeared, fringing frame around it.\n\n"
                      + "The circle is a circle at any aspect ratio, and both "
                      + "its centre and its radius are fractions of the canvas "
                      + "— the radius of its height — so it sits in the same "
                      + "place at every export resolution.")
        target: root.node
        propertyName: "maskEnabled"
    }
    ToggleRow {
        visible: root.node ? root.node.maskEnabled === true : false
        sub: true
        label: qsTr("Invert")
        info: qsTr("Off blurs what is inside the circle. On blurs everything "
                   + "outside it and leaves the inside sharp.")
        target: root.node
        propertyName: "maskInvert"
    }
    ParamSlider {
        visible: root.node ? root.node.maskEnabled === true : false
        label: qsTr("Centre X")
        info: qsTr("Where the circle sits across the canvas: 0 is the left "
                   + "edge, 1 the right.")
        target: root.node; propertyName: "maskX"
        from: 0; to: 1
    }
    ParamSlider {
        visible: root.node ? root.node.maskEnabled === true : false
        label: qsTr("Centre Y")
        info: qsTr("Where the circle sits down the canvas: 0 is the top, 1 the "
                   + "bottom.")
        target: root.node; propertyName: "maskY"
        from: 0; to: 1
    }
    ParamSlider {
        visible: root.node ? root.node.maskEnabled === true : false
        label: qsTr("Radius")
        info: qsTr("The size of the circle, as a fraction of the canvas "
                   + "height.")
        target: root.node; propertyName: "maskRadius"
        from: 0; to: 1
    }
    ParamSlider {
        visible: root.node ? root.node.maskEnabled === true : false
        label: qsTr("Soft edge")
        info: qsTr("How gradually the effect falls off at the circle's edge, "
                   + "as a fraction of its radius.")
        details: qsTr("0 is a hard cut. 1 spreads the transition from the "
                      + "centre all the way out to twice the radius, which is "
                      + "what stops the edge of the circle reading as an edge "
                      + "in the picture.")
        target: root.node; propertyName: "maskSoftness"
        from: 0; to: 1
    }
}
