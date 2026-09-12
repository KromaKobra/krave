// A flat colour filling the node's box.
//
// Colour only. Still images used to live here, behind imageSource/fillMode; they
// belong to an Image node, and folding them in was what made a background do two
// unrelated things.
import QtQuick

LayerFrame {
    id: frame

    Rectangle {
        anchors.fill: parent
        color: frame.layerData ? frame.layerData.color : "transparent"
    }
}
