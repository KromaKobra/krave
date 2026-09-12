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
//       effects  - the blur nodes' shader passes, if any; content is reparented
//                  INSIDE them (see contentHost)
//       canvas   - `canvasContent`: things that must be free to draw anywhere
//                  rather than be clipped to the node's own box
//       box      - `content` (default): the layer's placed, rotatable rectangle
//
// The effect chain sits INSIDE `group`, so a layer is blurred before its blend
// texture is taken and before the enclosing Transform matrix on `frame` is
// applied. A blurred layer inside a scaled Transform therefore has its blur --
// and its mask circle -- scaled with the group, which is the same thing `add`
// and `screen` already do and the only reading under which a transform moves a
// group as one unit.
//
// The backing object is called `layerData`, not `layer`: QQuickItem already
// declares a FINAL `layer` property (the layer.enabled/layer.effect group), and
// shadowing it is a hard QML error.
import QtQuick

Item {
    id: frame

    property var layerData: null

    /// The transform nodes enclosing this layer, OUTERMOST FIRST -- straight from
    /// RenderList's "layerTransforms" role. Empty for a layer under none.
    ///
    /// The transform sits on the ROOT item rather than on `group`, so it covers
    /// canvasArea as well as box, and so the blend-mode texture on `group` is
    /// captured BEFORE the transform rather than after -- a rotated layer must
    /// blend with what is under it, not blend a rotated texture.
    property var layerTransforms: []

    /// The blur nodes enclosing this layer, OUTERMOST FIRST -- RenderList's
    /// "layerEffects" role. Empty for a layer under none, which is the case
    /// that must cost nothing: no Loader item, no offscreen buffer.
    ///
    /// Unlike layerTransforms these are applied PER LAYER rather than to the
    /// group as a whole, which is an approximation rather than an identity. See
    /// BlurNode's header.
    property var layerEffects: []

    /// Where canvasArea and box actually live: inside the effect chain when
    /// there is one, directly in `group` when there is not.
    readonly property Item contentHost: (frame.layerEffects && frame.layerEffects.length > 0
                                         && effectsLoader.item)
                                        ? effectsLoader.item.hole : group

    /// The layer's placed box, in canvas pixels.
    default property alias content: box.data

    /// Content that spans the whole canvas rather than the layer box.
    property alias canvasContent: canvasArea.data

    readonly property alias box: box
    readonly property string blendMode: layerData ? layerData.blendMode : "normal"

    /// Compose the enclosing transforms into one matrix, outermost first.
    ///
    /// Plain arithmetic over the node properties and frame.width/frame.height on
    /// purpose: reading `t.offsetX` here registers a real QML dependency, so the
    /// binding re-evaluates when a slider moves. Anything routed through
    /// mapToItem would NOT -- see GraphLayout.qml's header for the same trap.
    function composedMatrix() {
        const m = Qt.matrix4x4()
        const list = frame.layerTransforms
        if (!list)
            return m

        for (let i = 0; i < list.length; ++i) {
            const t = list[i]
            if (!t)
                continue

            const px = t.pivotX * frame.width
            const py = t.pivotY * frame.height

            // Offset last so it reads as a plain screen-space nudge rather than
            // as something the scale and rotation also act on.
            m.translate(Qt.vector3d(t.offsetX * frame.width,
                                    t.offsetY * frame.height, 0))
            m.translate(Qt.vector3d(px, py, 0))
            m.rotate(t.rotation, Qt.vector3d(0, 0, 1))
            m.scale(t.scaleX, t.scaleY, 1)
            m.translate(Qt.vector3d(-px, -py, 0))
        }
        return m
    }

    transform: Matrix4x4 { matrix: frame.composedMatrix() }

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

        Loader {
            id: effectsLoader

            anchors.fill: parent
            active: frame.layerEffects && frame.layerEffects.length > 0
            sourceComponent: blurChain
        }

        // `parent` is a BINDING, not a fixed position in the tree: it moves
        // these two inside the effect chain when one appears and back out when
        // it goes, with no imperative reparenting anywhere. z is explicit
        // because reparenting reorders children, and the box must stay in front
        // of the canvas-spanning content whatever order they arrive in.
        Item {
            id: canvasArea

            parent: frame.contentHost
            anchors.fill: parent
            z: 0
        }

        Item {
            id: box

            parent: frame.contentHost
            z: 1
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
        id: blurChain
        BlurChain {
            effects: frame.layerEffects
            depth: 0
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
