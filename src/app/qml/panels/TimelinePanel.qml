// Transport and timeline.
//
// The waveform is painted once per track from a precomputed min/max envelope
// (AudioEngine::waveform), never from the full PCM array. Playback progress is a
// clip rectangle over a second copy of that painting, so advancing the playhead
// costs no repaint at all.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import MusicVisualizer
import "../controls"

Rectangle {
    id: root

    readonly property real duration: audioEngine.duration
    readonly property bool ready: audioEngine.hasTrack && duration > 0

    implicitHeight: Theme.timelineHeight
    color: Theme.surface

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

    function formatClock(seconds) {
        const total = Math.round(seconds)
        return Math.floor(total / 60) + ":" + String(total % 60).padStart(2, "0")
    }

    /// Tick spacing that lands on a round number of seconds and leaves roughly
    /// `targetCount` labels across the strip.
    function tickStep(targetCount) {
        if (!ready || targetCount < 1)
            return 1
        const raw = duration / targetCount
        const steps = [0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600]
        for (let i = 0; i < steps.length; ++i) {
            if (raw <= steps[i])
                return steps[i]
        }
        return 900
    }

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.divider
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.space3
        anchors.bottomMargin: Theme.space4
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        spacing: Theme.space3

        // ----------------------------------------------------------- transport
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            IconButton {
                iconPath: Icons.skipBack
                iconFilled: true
                iconSize: 14
                enabled: audioEngine.hasTrack
                tip: qsTr("Back to start")
                onClicked: audioEngine.seek(0)
            }

            IconButton {
                iconPath: audioEngine.playing ? Icons.pause : Icons.play
                iconFilled: true
                iconSize: 14
                diameter: 36
                circular: true
                outlined: true
                enabled: audioEngine.hasTrack
                tip: audioEngine.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                onClicked: audioEngine.togglePlay()
            }

            IconButton {
                iconPath: Icons.stop
                iconFilled: true
                iconSize: 13
                enabled: audioEngine.hasTrack
                tip: qsTr("Stop")
                onClicked: audioEngine.stop()
            }

            Rectangle {
                Layout.leftMargin: Theme.space2
                Layout.rightMargin: Theme.space2
                Layout.preferredWidth: 1
                Layout.preferredHeight: 18
                color: Theme.divider
            }

            Text {
                text: root.formatTime(audioEngine.position)
                color: Theme.text
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeBody
            }

            Text {
                text: "/ " + root.formatTime(root.duration)
                color: Theme.textGhost
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeBody
            }

            Item { Layout.fillWidth: true }

            Tag {
                visible: audioEngine.hasTrack
                text: audioEngine.trackTitle
            }

            IconButton {
                iconPath: audioEngine.volume > 0 ? Icons.speaker : Icons.speakerMute
                iconSize: 15
                Layout.leftMargin: Theme.space2
                tip: audioEngine.volume > 0 ? qsTr("Mute") : qsTr("Unmute")

                property real restoreVolume: 1

                onClicked: {
                    if (audioEngine.volume > 0) {
                        restoreVolume = audioEngine.volume
                        audioEngine.volume = 0
                    } else {
                        audioEngine.volume = restoreVolume > 0 ? restoreVolume : 1
                    }
                }
            }

            AppSlider {
                Layout.preferredWidth: 84
                trackHeight: 3
                handleSize: 11
                from: 0
                to: 1
                value: audioEngine.volume
                onMoved: audioEngine.volume = value
            }
        }

        // ------------------------------------------------------------- ruler
        Item {
            id: ruler

            Layout.fillWidth: true
            Layout.preferredHeight: root.ready ? 14 : 0
            visible: root.ready

            readonly property real step: root.tickStep(Math.max(2, Math.round(width / 100)))

            Repeater {
                model: root.ready ? Math.floor(root.duration / ruler.step) + 1 : 0

                delegate: Item {
                    required property int index

                    readonly property real seconds: index * ruler.step
                    x: (seconds / root.duration) * ruler.width
                    height: ruler.height

                    Rectangle {
                        width: 1
                        height: 4
                        anchors.bottom: parent.bottom
                        color: Theme.withAlpha(Theme.text, 0.22)
                    }

                    Text {
                        x: 4
                        // Drop the last label rather than let it run off the end.
                        visible: parent.x + x + implicitWidth <= ruler.width
                        text: root.formatClock(parent.seconds)
                        color: Theme.textGhost
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSizeKicker
                    }
                }
            }
        }

        // ---------------------------------------------------------- waveform
        Item {
            id: strip

            Layout.fillWidth: true
            Layout.fillHeight: true

            function timeToX(t) {
                return root.ready ? (t / root.duration) * width : 0
            }

            function xToTime(x) {
                return root.ready
                    ? Math.max(0, Math.min(root.duration, (x / width) * root.duration)) : 0
            }

            readonly property real playheadX: timeToX(audioEngine.position)

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSm
                color: Theme.bg
                clip: true

                // Unplayed: a quiet neutral envelope.
                WaveformStrip {
                    anchors.fill: parent
                    fill: Theme.neutral700
                }

                // Played: the same envelope in the accent, revealed by a clip that
                // tracks the playhead. Moving it repaints nothing.
                Item {
                    width: Math.max(0, strip.playheadX)
                    height: parent.height
                    clip: true

                    WaveformStrip {
                        width: strip.width
                        height: parent.height
                        fill: Theme.accent400
                    }
                }
            }

            Rectangle {
                visible: root.ready
                width: 2
                radius: 1
                color: Theme.accent
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                x: strip.playheadX - width / 2

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: Theme.accent
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: -3
                }
            }

            Text {
                anchors.centerIn: parent
                visible: !audioEngine.hasTrack
                text: audioEngine.loading
                      ? qsTr("Decoding audio…")
                      : qsTr("Import an audio track to begin")
                color: Theme.textGhost
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            MouseArea {
                id: scrubArea

                anchors.fill: parent
                enabled: root.ready
                cursorShape: root.ready ? Qt.PointingHandCursor : Qt.ArrowCursor

                // Scrubbing is silent: stop the sink once on press, move the playhead
                // freely while dragging, and pick playback back up on release. Seeking
                // while paused touches no audio device, so the drag stays smooth.
                property bool resumeAfterScrub: false

                function finishScrub() {
                    if (!resumeAfterScrub)
                        return
                    resumeAfterScrub = false
                    audioEngine.play()
                }

                onPressed: (mouse) => {
                    resumeAfterScrub = audioEngine.playing
                    if (resumeAfterScrub)
                        audioEngine.pause()
                    audioEngine.seek(strip.xToTime(mouse.x))
                }
                onPositionChanged: (mouse) => {
                    if (pressed)
                        audioEngine.seek(strip.xToTime(mouse.x))
                }
                onReleased: scrubArea.finishScrub()
                onCanceled: scrubArea.finishScrub()
            }
        }
    }
}
