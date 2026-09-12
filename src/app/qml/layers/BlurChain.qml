// The shader passes for ONE blur node, wrapped around the passes for the next.
//
// A layer can be enclosed by several blur nodes (two of them on the same path is
// legal wiring), so this file recurses: level `depth` builds its own passes and
// then loads another copy of itself at `depth + 1` inside them. `hole` resolves
// to the innermost level's innermost Item, and that is where the actual content
// goes -- LayerFrame binds its canvasArea and box to it. Content therefore sits
// under every level's passes at once, innermost node applied first.
//
// Structure at each level, outermost first:
//
//   blurV   vertical half of the separable blur
//     blurH horizontal half
//       split  radial RGB separation
//         the next level, or the hole
//
// The separable pair is nested rather than sequential because Qt Quick has no
// notion of a pass chain: `layer.enabled` renders an Item's subtree to a texture
// and hands that texture to one ShaderEffect, so a second pass is expressed as a
// second Item wrapping the first. Each enabled level costs one offscreen buffer,
// which is why every one of them is switched off when its effect is.
//
// The separation is INSIDE the blur, so the fringe it produces gets softened
// along with everything else -- which is what makes the two read as one lens
// artefact rather than as a sharp colour edge sitting on top of a blur.
//
// Sizes come from `chain.height`, not from the layer's box: every radius here is
// a fraction of the CANVAS height, so the same project renders identically at
// preview and at export resolution.
import QtQuick

Item {
    id: chain

    /// The blur nodes enclosing this layer, outermost first -- RenderList's
    /// "layerEffects" role, passed down unchanged at every level.
    property var effects: []
    /// Which of them this level draws. The Loader below builds `depth + 1`.
    property int depth: 0

    /// The node this level belongs to, or null if the list ran out (which only
    /// happens transiently while the model is being reset).
    readonly property var node: (chain.effects && chain.depth < chain.effects.length)
                                ? chain.effects[chain.depth] : null
    readonly property bool hasInner: chain.effects
                                     && chain.depth + 1 < chain.effects.length

    /// Where content belongs: the innermost level's innermost Item. Falls back
    /// to this level's own while a nested level is still loading, and rebinds
    /// when it arrives -- which is exactly why this is a binding and not an
    /// assignment.
    readonly property Item hole: (chain.hasInner && innerLoader.item)
                                 ? innerLoader.item.hole : split

    // A blur pass is only worth its offscreen buffer when something is actually
    // being blurred. Both the Gaussian and the chroma-only blur run in the same
    // shader, so either one switches the pair on.
    readonly property bool blurOn: chain.node
                                   && (chain.node.gaussianEnabled
                                       || chain.node.chromaBlurEnabled)
    readonly property bool splitOn: chain.node ? chain.node.chromaSplitEnabled : false

    anchors.fill: parent

    Item {
        id: blurV

        anchors.fill: parent
        layer.enabled: chain.blurOn
        layer.effect: verticalBlur
    }

    // Declared outside blurV so the nesting below reads as one list rather than
    // as four levels of indentation; reparented on creation instead.
    Item {
        id: blurH

        parent: blurV
        anchors.fill: parent
        layer.enabled: chain.blurOn
        layer.effect: horizontalBlur
    }

    Item {
        id: split

        parent: blurH
        anchors.fill: parent
        layer.enabled: chain.splitOn
        layer.effect: chromaSplit
    }

    Loader {
        id: innerLoader

        parent: split
        anchors.fill: parent
        active: chain.hasInner
        // By file name rather than by type name: a QML file cannot instantiate
        // itself directly, and a Loader is the supported way to tie the knot.
        source: "BlurChain.qml"
        onLoaded: {
            // Qt.binding, not assignment: the enclosing list is handed to the
            // delegate after construction, so a plain copy taken here would be
            // whatever the list was at load time.
            item.effects = Qt.binding(() => chain.effects)
            item.depth = chain.depth + 1
        }
    }

    // -------------------------------------------------------------- the passes
    //
    // Ordinary Components rather than inline components, because these have to
    // read `chain` -- an inline component cannot see the ids of the document it
    // is declared in.

    Component {
        id: verticalBlur
        BlurPass {
            node: chain.node
            canvasW: chain.width
            canvasH: chain.height
            dir: Qt.vector2d(0, 1)
        }
    }

    Component {
        id: horizontalBlur
        BlurPass {
            node: chain.node
            canvasW: chain.width
            canvasH: chain.height
            dir: Qt.vector2d(1, 0)
        }
    }

    // Only one instance, so its uniforms are written here rather than in a file
    // of their own. Names match chroma_split.frag exactly.
    Component {
        id: chromaSplit
        ShaderEffect {
            property real splitPx: chain.node
                                   ? chain.node.chromaSplitAmount * chain.height : 0

            property vector2d texSize: Qt.vector2d(Math.max(chain.width, 1),
                                                   Math.max(chain.height, 1))
            property vector2d maskCentre: chain.node
                                          ? Qt.vector2d(chain.node.maskX, chain.node.maskY)
                                          : Qt.vector2d(0.5, 0.5)
            property real maskRadius: chain.node ? chain.node.maskRadius : 0.3
            property real maskSoft: chain.node ? chain.node.maskSoftness : 0.5
            property real maskInvert: chain.node && chain.node.maskInvert ? 1 : 0
            property real maskOn: chain.node && chain.node.maskEnabled ? 1 : 0

            fragmentShader: "qrc:/qt/qml/krave/shaders/chroma_split.frag.qsb"
        }
    }
}
