import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Combo box bound to a string-valued property on a backend object.
ColumnLayout {
    id: root

    property string label: ""
    property QtObject target: null
    property string propertyName: ""
    /// List of { text, value } entries.
    property var options: []

    spacing: 2
    enabled: target !== null

    Text {
        text: root.label
        color: "#c8c8d4"
        font.pixelSize: 11
    }

    ComboBox {
        id: combo
        Layout.fillWidth: true
        implicitHeight: 26
        model: root.options
        textRole: "text"
        valueRole: "value"

        currentIndex: {
            if (!root.target || !root.propertyName)
                return -1
            const current = root.target[root.propertyName]
            for (let i = 0; i < root.options.length; ++i) {
                if (root.options[i].value === current)
                    return i
            }
            return -1
        }

        onActivated: (index) => {
            if (root.target && root.propertyName)
                root.target[root.propertyName] = root.options[index].value
        }

        contentItem: Text {
            leftPadding: 8
            rightPadding: 24
            text: combo.displayText
            color: "#e0e0ea"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: "#1c1c26"
            border.color: combo.activeFocus ? "#4cc9f0" : "#2f2f3d"
            border.width: 1
            radius: 3
        }
    }
}
