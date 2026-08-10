import QtQuick

import MusicVisualizer

// One painting of the track's amplitude envelope, in a single colour.
//
// The timeline stacks two of these -- a neutral one and an accent one revealed by
// a clip at the playhead -- so playback progress never triggers a repaint.
Canvas {
    id: root

    property color fill: Theme.neutral700
    /// Centre-to-centre distance between bars, in pixels.
    property real pitch: 3
    property real barWidth: 2

    onFillChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    Connections {
        target: audioEngine
        function onTrackChanged() { root.requestPaint() }
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)

        const data = audioEngine.waveform
        if (!data || data.length < 2 || width <= 0)
            return

        const buckets = data.length / 2
        const mid = height / 2
        ctx.fillStyle = root.fill

        for (let x = 0; x + barWidth <= width; x += pitch) {
            // Several buckets can land under one bar; take the extremes so
            // transients stay visible instead of averaging away.
            const from = Math.floor(x / width * buckets)
            const to = Math.max(from + 1, Math.floor((x + pitch) / width * buckets))

            let peak = 0
            for (let b = from; b < to && b < buckets; ++b) {
                peak = Math.max(peak, Math.abs(data[b * 2]), Math.abs(data[b * 2 + 1]))
            }

            const h = Math.max(2, peak * (height - 4))
            ctx.fillRect(x, mid - h / 2, barWidth, h)
        }
    }
}
