import QtQuick

LayerFrame {
    id: frame

    Rectangle {
        anchors.fill: parent
        color: frame.layerData ? frame.layerData.color : "transparent"
    }

    Image {
        anchors.fill: parent
        source: frame.layerData && frame.layerData.imageSource ? frame.layerData.imageSource : ""
        visible: status === Image.Ready
        asynchronous: true
        // Decode at the size actually needed; matters at export resolutions.
        sourceSize.width: width
        sourceSize.height: height
        fillMode: {
            if (!frame.layerData)
                return Image.PreserveAspectCrop
            switch (frame.layerData.fillMode) {
            case "stretch": return Image.Stretch
            case "fit": return Image.PreserveAspectFit
            default: return Image.PreserveAspectCrop
            }
        }
    }
}
