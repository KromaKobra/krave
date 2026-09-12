// A still picture from disk, cropped and placed in the node's box.
//
// The crop is done by GEOMETRY rather than by Image.sourceClipRect: the node
// stores the crop normalized to the source, and sourceClipRect wants source
// pixels, which would mean reading back the decoded image's dimensions and
// reacting to them. Instead the Image is blown up to the reciprocal of the crop
// size inside a clipping box and shifted so the wanted region lands in view --
// which needs no source dimensions at all, and leaves `sourceSize` tracking the
// ZOOMED size so a tight crop still decodes at full detail.
//
// Deliberately NOT `asynchronous`. Local files load synchronously by default, and
// an async load would let export frame 0 render before the picture arrived --
// which is exactly the guarantee that frame 0 matches the preview.
import QtQuick

LayerFrame {
    id: frame

    /// Guarded against 0 both to avoid dividing by it and because a zero-width
    /// crop has no sensible picture to show anyway.
    readonly property real cropW: frame.layerData
                                  ? Math.max(frame.layerData.cropW, 0.01) : 1
    readonly property real cropH: frame.layerData
                                  ? Math.max(frame.layerData.cropH, 0.01) : 1
    readonly property real cropX: frame.layerData ? frame.layerData.cropX : 0
    readonly property real cropY: frame.layerData ? frame.layerData.cropY : 0

    Item {
        anchors.fill: parent
        clip: true

        Image {
            source: frame.layerData && frame.layerData.source
                    ? frame.layerData.source : ""
            visible: status === Image.Ready

            width: parent.width / frame.cropW
            height: parent.height / frame.cropH
            x: -frame.cropX * width
            y: -frame.cropY * height

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
}
