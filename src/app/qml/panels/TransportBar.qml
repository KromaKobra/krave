import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: root

    spacing: 10

    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds < 0)
            seconds = 0
        const total = Math.floor(seconds)
        const m = Math.floor(total / 60)
        const s = total % 60
        const cs = Math.floor((seconds - total) * 100)
        return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0")
             + "." + String(cs).padStart(2, "0")
    }

    component TransportButton: Button {
        implicitWidth: 38
        implicitHeight: 28
        contentItem: Text {
            text: parent.text
            color: parent.enabled ? "#e0e0ea" : "#4a4a58"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 4
            color: parent.down ? "#2f2f3d" : (parent.hovered ? "#26263a" : "#1c1c26")
            border.color: "#2f2f3d"
            border.width: 1
        }
    }

    TransportButton {
        text: audioEngine.playing ? "❚❚" : "▶"
        enabled: audioEngine.hasTrack
        onClicked: audioEngine.togglePlay()
    }

    TransportButton {
        text: "■"
        enabled: audioEngine.hasTrack
        onClicked: audioEngine.stop()
    }

    Text {
        text: root.formatTime(audioEngine.position) + " / " + root.formatTime(audioEngine.duration)
        color: "#a0a0b4"
        font.pixelSize: 12
        font.family: "Consolas"
    }

    Text {
        Layout.fillWidth: true
        text: audioEngine.hasTrack ? audioEngine.trackTitle : ""
        color: "#6a6a80"
        font.pixelSize: 12
        elide: Text.ElideMiddle
        horizontalAlignment: Text.AlignHCenter
    }

    Text {
        text: qsTr("Volume")
        color: "#6a6a80"
        font.pixelSize: 11
    }

    Slider {
        Layout.preferredWidth: 90
        from: 0
        to: 1
        value: audioEngine.volume
        onMoved: audioEngine.volume = value

        implicitHeight: 20

        background: Rectangle {
            x: parent.leftPadding
            y: parent.topPadding + parent.availableHeight / 2 - height / 2
            width: parent.availableWidth
            height: 3
            radius: 2
            color: "#2a2a38"
            Rectangle {
                width: parent.parent.visualPosition * parent.width
                height: parent.height
                color: "#4cc9f0"
                radius: 2
            }
        }

        handle: Rectangle {
            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
            y: parent.topPadding + parent.availableHeight / 2 - height / 2
            width: 12
            height: 12
            radius: 6
            color: "#e0e0ea"
        }
    }
}
