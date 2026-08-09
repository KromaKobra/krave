// Particle burst driven by one frequency band.
//
// Emission rate is the layer's base rate plus its reactive rate scaled by the
// band energy, so the layer idles quietly and bursts on hits.
//
// The layer box is the EMITTER region -- usually a thin band. Everything lives in
// `canvasContent`, spanning the whole canvas, for two reasons:
//
//  1. Particles are meant to fly out of the emitter band. Parented to the box
//     they would be clipped at its edge the moment they left it (the blend-mode
//     texture and the painter bounds both cut them off).
//  2. A ParticleSystem interprets an Emitter's position in the SYSTEM's
//     coordinates, not the emitter's own parent's. An emitter parented to the
//     box would spawn particles at the canvas origin instead of in the band.
//
// So the emitter tracks `frame.box`'s geometry explicitly while living in the
// same coordinate space as the system.
import QtQuick
import QtQuick.Particles

LayerFrame {
    id: frame

    readonly property real level: layerData ? layerData.level : 0

    canvasContent: [
        // The system advances on the Qt animation clock. During export that
        // clock is driven a fixed 1/fps step per rendered frame (see
        // ExportController's animation driver), so particle motion in the file
        // matches the preview instead of running at whatever speed the renderer
        // happened to achieve.
        ParticleSystem {
            id: system
            anchors.fill: parent
            running: true
        },

        ImageParticle {
            system: system
            anchors.fill: parent
            // ImageParticle's default built-in sprite (qrc:///particleresources/
            // glowdot.png) silently fails to load in this Qt build -- particles
            // simulate but draw nothing -- so the app ships its own.
            source: "qrc:/qt/qml/MusicVisualizer/assets/glowdot.png"
            color: frame.layerData ? frame.layerData.color : "white"
            colorVariation: 0.25
            entryEffect: ImageParticle.Scale
        },

        Emitter {
            system: system

            // Mirrors the layer box, but in the system's coordinate space.
            x: frame.box.x
            y: frame.box.y
            width: frame.box.width
            height: frame.box.height

            emitRate: {
                if (!frame.layerData)
                    return 0
                return Math.max(0, frame.layerData.baseRate
                                   + frame.layerData.reactiveRate * frame.level)
            }
            lifeSpan: frame.layerData ? frame.layerData.lifeSpan : 1600
            lifeSpanVariation: lifeSpan * 0.3
            size: frame.layerData ? frame.layerData.particleSize : 14
            sizeVariation: size * 0.5
            endSize: 0

            velocity: AngleDirection {
                angle: frame.layerData ? frame.layerData.angle : 270
                angleVariation: frame.layerData ? frame.layerData.angleSpread : 40
                magnitude: frame.layerData ? frame.layerData.speed : 220
                magnitudeVariation: magnitude * 0.4
            }

            acceleration: PointDirection {
                y: frame.layerData ? frame.layerData.gravity : 90
            }
        }
    ]
}
