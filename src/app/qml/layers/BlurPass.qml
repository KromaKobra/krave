// One axis of a blur node's separable blur, as a `layer.effect`.
//
// A file of its own rather than an inline component in BlurChain, for two
// reasons: an inline component cannot see the ids of the document it is declared
// in, and the horizontal and vertical passes differ ONLY in `dir` -- so the ten
// uniforms they share are written once here instead of twice there.
//
// The amounts arrive as fractions of canvas height and leave as pixels: that
// conversion happens here, at the one place that knows both, so the shader never
// has to learn the render resolution.
import QtQuick

ShaderEffect {
    id: pass

    /// The BlurNode whose settings this pass reads.
    property var node: null
    /// The canvas size in pixels. Not `pass.width`/`height`: those are the
    /// layered item's, which is the same thing today and would stop being so
    /// the moment anything downsamples the texture.
    property real canvasW: 1
    property real canvasH: 1

    /// vec2(1,0) for the horizontal pass, vec2(0,1) for the vertical one.
    property vector2d dir: Qt.vector2d(1, 0)

    // ------------------------------------------------------ shader uniforms
    //
    // Names match blur_separable.frag exactly; ShaderEffect binds them by name.

    property real gaussPx: pass.node && pass.node.gaussianEnabled
                           ? pass.node.gaussianAmount * pass.canvasH : 0
    property real chromaPx: pass.node ? pass.node.chromaBlurAmount * pass.canvasH : 0
    property real chromaOn: pass.node && pass.node.chromaBlurEnabled ? 1 : 0

    property vector2d texSize: Qt.vector2d(Math.max(pass.canvasW, 1),
                                           Math.max(pass.canvasH, 1))
    property vector2d maskCentre: pass.node
                                  ? Qt.vector2d(pass.node.maskX, pass.node.maskY)
                                  : Qt.vector2d(0.5, 0.5)
    property real maskRadius: pass.node ? pass.node.maskRadius : 0.3
    property real maskSoft: pass.node ? pass.node.maskSoftness : 0.5
    property real maskInvert: pass.node && pass.node.maskInvert ? 1 : 0
    property real maskOn: pass.node && pass.node.maskEnabled ? 1 : 0

    fragmentShader: "qrc:/qt/qml/krave/shaders/blur_separable.frag.qsb"
}
