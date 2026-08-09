import QtQuick

LayerFrame {
    id: frame

    Text {
        anchors.fill: parent
        text: frame.layerData ? frame.layerData.text : ""
        color: frame.layerData ? frame.layerData.color : "white"
        // Size is a fraction of the CANVAS height, not this layer's height, so
        // text scales with output resolution instead of with the layer box.
        // frame.height is the canvas; the enclosing box is the layer.
        font.pixelSize: Math.max(1, (frame.layerData ? frame.layerData.fontScale : 0.06)
                                 * frame.height)
        font.family: frame.layerData ? frame.layerData.fontFamily : "Segoe UI"
        font.bold: frame.layerData ? frame.layerData.bold : false
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: {
            if (!frame.layerData)
                return Text.AlignHCenter
            switch (frame.layerData.alignment) {
            case "left": return Text.AlignLeft
            case "right": return Text.AlignRight
            default: return Text.AlignHCenter
            }
        }
        elide: Text.ElideRight
    }
}
