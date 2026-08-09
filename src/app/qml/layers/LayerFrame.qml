// Shared placement for every layer type.
//
// Turns a layer's normalized posX/posY/sizeW/sizeH into pixels against whatever
// canvas size it finds itself in. Subtypes fill in `content`.
//
// The root Item deliberately does NOT carry the layer's size. A Loader always
// overwrites its loaded item's width and height, which silently clobbers any
// binding put on them -- the layer would end up canvas-sized and simply clipped.
// So the root spans the canvas (which is what the Loader wants anyway) and the
// layer's own box is an inner Item. `frame.width`/`frame.height` are therefore
// always the CANVAS size, which is what resolution-independent sizing needs.
//
// The backing object is called `layerData`, not `layer`: QQuickItem already
// declares a FINAL `layer` property (the layer.enabled/layer.effect group), and
// shadowing it is a hard QML error.
//
// blendMode is carried on the layer but only "normal" is honoured today; Qt Quick
// has no per-item blend mode, so the others need a ShaderEffect pass.
import QtQuick

Item {
    id: frame

    property var layerData: null
    default property alias content: box.data

    /// The layer's placed box, in canvas pixels.
    readonly property alias box: box

    Item {
        id: box

        visible: frame.layerData ? frame.layerData.layerVisible : false
        opacity: frame.layerData ? frame.layerData.layerOpacity : 1.0
        rotation: frame.layerData ? frame.layerData.layerRotation : 0

        width: frame.layerData ? frame.layerData.sizeW * frame.width : 0
        height: frame.layerData ? frame.layerData.sizeH * frame.height : 0
        x: frame.layerData ? (frame.layerData.posX - frame.layerData.sizeW / 2) * frame.width : 0
        y: frame.layerData ? (frame.layerData.posY - frame.layerData.sizeH / 2) * frame.height : 0
    }
}
