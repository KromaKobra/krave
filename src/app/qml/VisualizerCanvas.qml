// The visualizer itself.
//
// This one file is instantiated TWICE: once inside the main window for live
// preview, and once inside the offscreen export window at full output resolution.
// Nothing in here knows or cares which one it is -- it just renders the layer
// stack at whatever size it has been given. That is what keeps the exported video
// identical to what the user previewed.
//
// All layer geometry is normalized (0..1), so the same project renders correctly
// at 640x360 preview and 3840x2160 export.
import QtQuick
import "layers"

Item {
    id: root

    /// The LayerModel to render. Index 0 is the bottom of the stack.
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
            required property int index

            anchors.fill: parent
            // Repeater emits in model order and index 0 must be furthest back.
            z: index
            active: layerObject !== null

            sourceComponent: {
                if (!layerObject)
                    return null
                switch (layerObject.typeId) {
                case "bars": return barsComponent
                case "background": return backgroundComponent
                case "text": return textComponent
                default: return null
                }
            }

            onLoaded: item.layerData = layerLoader.layerObject
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
}
