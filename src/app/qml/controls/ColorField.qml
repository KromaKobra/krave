import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// Colour swatch that opens a colour dialog, bound to a QColor property.
ColumnLayout {
    id: root

    property string label: ""
    property QtObject target: null
    property string propertyName: ""

    readonly property color value: {
        if (!target || !propertyName)
            return "#000000"
        const v = target[propertyName]
        return v === undefined ? "#000000" : v
    }

    spacing: 2
    enabled: target !== null

    Text {
        text: root.label
        color: "#c8c8d4"
        font.pixelSize: 11
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 26
        radius: 3
        color: root.value
        border.color: "#2f2f3d"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: root.value.toString()
            font.pixelSize: 10
            font.family: "Consolas"
            // Flip the caption against the swatch so it stays legible either way.
            color: (root.value.r * 0.299 + root.value.g * 0.587 + root.value.b * 0.114) > 0.55
                   ? "#000000" : "#ffffff"
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: dialog.open()
        }
    }

    ColorDialog {
        id: dialog
        selectedColor: root.value
        onAccepted: {
            if (root.target && root.propertyName)
                root.target[root.propertyName] = selectedColor
        }
    }
}
