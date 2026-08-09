// Waveform strip with a draggable playhead.
//
// The waveform is painted once per track from a precomputed min/max envelope
// (AudioEngine::waveform), never from the full PCM array. The playhead is a plain
// Rectangle on top, so moving it costs no repaint of the waveform.
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    implicitHeight: 72

    readonly property real duration: audioEngine.duration
    readonly property bool ready: audioEngine.hasTrack && duration > 0

    function timeToX(t) {
        return ready ? (t / duration) * width : 0
    }

    function xToTime(x) {
        return ready ? Math.max(0, Math.min(duration, (x / width) * duration)) : 0
    }

    Rectangle {
        anchors.fill: parent
        color: "#12121a"
        border.color: "#22222e"
        border.width: 1
    }

    Canvas {
        id: waveCanvas
        anchors.fill: parent
        anchors.margins: 1

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)

            const data = audioEngine.waveform
            if (!data || data.length < 2)
                return

            const buckets = data.length / 2
            const mid = height / 2
            ctx.fillStyle = "#3a6ea5"

            for (let x = 0; x < width; ++x) {
                // Several buckets can land on one pixel column; take the extremes
                // so transients stay visible instead of averaging away.
                const from = Math.floor(x / width * buckets)
                const to = Math.max(from + 1, Math.floor((x + 1) / width * buckets))

                let lo = 0
                let hi = 0
                for (let b = from; b < to && b < buckets; ++b) {
                    lo = Math.min(lo, data[b * 2])
                    hi = Math.max(hi, data[b * 2 + 1])
                }

                const y0 = mid - hi * mid
                const y1 = mid - lo * mid
                ctx.fillRect(x, y0, 1, Math.max(1, y1 - y0))
            }
        }
    }

    Connections {
        target: audioEngine
        function onTrackChanged() { waveCanvas.requestPaint() }
    }

    // Progress shading up to the playhead.
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 1
        x: 1
        width: Math.max(0, root.timeToX(audioEngine.position))
        color: "#ffffff"
        opacity: 0.07
    }

    Rectangle {
        id: playhead
        visible: root.ready
        width: 2
        color: "#f72585"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: root.timeToX(audioEngine.position) - width / 2

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: "#f72585"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
        }
    }

    Text {
        visible: !audioEngine.hasTrack
        anchors.centerIn: parent
        text: audioEngine.loading ? qsTr("Decoding audio…") : qsTr("Import an audio track to begin")
        color: "#55556a"
        font.pixelSize: 12
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.ready
        cursorShape: root.ready ? Qt.PointingHandCursor : Qt.ArrowCursor
        // Scrub continuously while dragging, not just on release.
        onPressed: (mouse) => audioEngine.seek(root.xToTime(mouse.x))
        onPositionChanged: (mouse) => {
            if (pressed)
                audioEngine.seek(root.xToTime(mouse.x))
        }
    }
}
