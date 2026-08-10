// Shared placement, opacity and blending for every layer type.
//
// The root Item deliberately does NOT carry the layer's size. A Loader always
// overwrites its loaded item's width and height, which silently clobbers any
// binding put on them -- the layer would end up canvas-sized and simply clipped.
// So the root spans the canvas (which is what the Loader wants anyway) and the
// layer's own box is an inner Item. `frame.width`/`frame.height` are therefore
// always the CANVAS size, which is what resolution-independent sizing needs.
//
// Structure:
//   frame        - spans the canvas
//     group      - carries visibility, opacity and blend mode; canvas-sized, so
//                  the blend texture never clips content that leaves the box
//       canvas   - `canvasContent`: things that must be free to draw anywhere,
//                  e.g. particles that fly out of their emitter band
//       box      - `content` (default): the layer's placed, rotatable rectangle
//
// The backing object is called `layerData`, not `layer`: QQuickItem already
// declares a FINAL `layer` property (the layer.enabled/layer.effect group), and
// shadowing it is a hard QML error.
import QtQuick

Item {
    id: frame

    property var layerData: null

    /// The layer's placed box, in canvas pixels.
    default property alias content: box.data

    /// Content that spans the whole canvas rather than the layer box.
    property alias canvasContent: canvasArea.data

    readonly property alias box: box
    readonly property string blendMode: layerData ? layerData.blendMode : "normal"

    Item {
        id: group

        anchors.fill: parent
        visible: frame.layerData ? frame.layerData.layerVisible : false
        opacity: frame.layerData ? frame.layerData.layerOpacity : 1.0

        // Qt Quick has no per-item blend mode, so anything other than "normal"
        // routes through an offscreen texture and a ShaderEffect that exploits
        // premultiplied source-over compositing. See shaders/blend_add.frag.
        // Rendering to a texture costs an extra pass, so only do it when a
        // non-normal blend actually needs one.
        layer.enabled: frame.blendMode === "add" || frame.blendMode === "screen"
        layer.effect: frame.blendMode === "screen" ? screenBlend
                    : frame.blendMode === "add" ? addBlend
                    : null

        Item {
            id: canvasArea
            anchors.fill: parent
        }

        Item {
            id: box

            rotation: frame.layerData ? frame.layerData.layerRotation : 0
            width: frame.layerData ? frame.layerData.sizeW * frame.width : 0
            height: frame.layerData ? frame.layerData.sizeH * frame.height : 0
            x: frame.layerData
               ? (frame.layerData.posX - frame.layerData.sizeW / 2) * frame.width : 0
            y: frame.layerData
               ? (frame.layerData.posY - frame.layerData.sizeH / 2) * frame.height : 0
        }
    }

    Component {
        id: addBlend
        ShaderEffect {
            fragmentShader: "qrc:/qt/qml/krave/shaders/blend_add.frag.qsb"
        }
    }

    Component {
        id: screenBlend
        ShaderEffect {
            fragmentShader: "qrc:/qt/qml/krave/shaders/blend_screen.frag.qsb"
        }
    }
}
