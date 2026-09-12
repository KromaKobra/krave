// The visualizer itself.
//
// This one file is instantiated TWICE: once inside the main window for live
// preview, and once inside the offscreen export window at full output resolution.
// Nothing in here knows or cares which one it is -- it just renders the draw list
// at whatever size it has been given. That is what keeps the exported video
// identical to what the user previewed.
//
// All geometry is normalized (0..1), so the same project renders correctly at
// 640x360 preview and 3840x2160 export.
import QtQuick
import "layers"

Item {
    id: root

    /// The RenderList to draw: the visual nodes reaching the graph's Output,
    /// flattened, furthest back first. Index 0 is the bottom of the stack.
    property var layers: null
    property color canvasColor: "#0b0b12"

    Rectangle {
        anchors.fill: parent
        color: root.canvasColor
    }

    Repeater {
        model: root.layers

        delegate: Loader {
            id: layerLoader

            // The role is "layerObject" rather than "layer" because QQuickItem
            // declares a FINAL `layer` property that a delegate cannot shadow.
            required property var layerObject
            /// The transform nodes enclosing this layer, outermost first.
            required property var layerTransforms
            required property int index

            anchors.fill: parent
            // Repeater emits in model order and index 0 must be furthest back.
            z: index
            active: layerObject !== null

            sourceComponent: {
                if (!layerObject)
                    return null
                switch (layerObject.typeId) {
                case "spectrum": return barsComponent
                case "background": return backgroundComponent
                case "text": return textComponent
                case "image": return imageComponent
                // Only visual nodes reach a RenderList at all, so there is
                // nothing here for audio, separator, merge, transform or output.
                default: return null
                }
            }

            onLoaded: {
                item.layerData = layerLoader.layerObject
                item.layerTransforms = layerLoader.layerTransforms
            }
        }
    }

    Component {
        id: barsComponent
        BarLayerView {}
    }

    Component {
        id: backgroundComponent
        BackgroundLayerView {}
    }

    Component {
        id: textComponent
        TextLayerView {}
    }

    Component {
        id: imageComponent
        ImageLayerView {}
    }
}
