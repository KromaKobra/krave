#pragma once

#include <SpectrumAnalyzer.h>

#include <QColor>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace mv {

/// Base class for everything stacked on the visualizer canvas.
///
/// Properties are declared with Q_PROPERTY(... MEMBER ...) on purpose: it gives
/// QML two-way bindings and lets Project.cpp serialize by walking the meta-object,
/// so adding a property here costs no serialization code and no panel plumbing.
///
/// They all share one `changed` NOTIFY signal. QML re-evaluates the affected
/// bindings on it, and the slight over-invalidation is far cheaper than
/// hand-writing a signal per property.
class Layer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString typeId READ typeId CONSTANT)
    Q_PROPERTY(int layerId READ layerId CONSTANT)

    Q_PROPERTY(QString name MEMBER m_name NOTIFY changed)
    Q_PROPERTY(bool layerVisible MEMBER m_layerVisible NOTIFY changed)
    Q_PROPERTY(qreal layerOpacity MEMBER m_layerOpacity NOTIFY changed)
    Q_PROPERTY(QString blendMode MEMBER m_blendMode NOTIFY changed)

    // Geometry is normalized to the canvas (0..1) so a project renders the same
    // at 1280x720 preview and 3840x2160 export.
    Q_PROPERTY(qreal posX MEMBER m_posX NOTIFY changed)
    Q_PROPERTY(qreal posY MEMBER m_posY NOTIFY changed)
    Q_PROPERTY(qreal sizeW MEMBER m_sizeW NOTIFY changed)
    Q_PROPERTY(qreal sizeH MEMBER m_sizeH NOTIFY changed)
    Q_PROPERTY(qreal layerRotation MEMBER m_layerRotation NOTIFY changed)

public:
    explicit Layer(QObject *parent = nullptr);

    virtual QString typeId() const = 0;

    int layerId() const { return m_layerId; }
    void setLayerId(int id) { m_layerId = id; }

    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; Q_EMIT changed(); }
    bool layerVisible() const { return m_layerVisible; }
    qreal layerOpacity() const { return m_layerOpacity; }

    /// True when this layer consumes spectrum data and must be analyzed each frame.
    virtual bool needsSpectrum() const { return false; }

    /// False for layers that only condition data and draw nothing, so the canvas
    /// knows not to look for a delegate.
    virtual bool isVisual() const { return true; }

    /// Analyzer settings for this layer. Each spectrum-consuming layer gets its
    /// own analyzer, so band counts and smoothing are genuinely per-layer.
    virtual SpectrumParams analysisParams() const { return {}; }

    /// Receive this frame's analysis. Called on the GUI thread.
    virtual void applyFrame(const SpectrumFrame &) {}

Q_SIGNALS:
    void changed();

protected:
    int m_layerId = 0;
    QString m_name;
    bool m_layerVisible = true;
    qreal m_layerOpacity = 1.0;
    QString m_blendMode = QStringLiteral("normal");
    qreal m_posX = 0.5;
    qreal m_posY = 0.5;
    qreal m_sizeW = 1.0;
    qreal m_sizeH = 1.0;
    qreal m_layerRotation = 0.0;
};

/// Conditions the audio into the numbers the bars actually draw.
///
/// This exists as its own layer because "what the spectrum really is" and "what
/// looks good" are different problems. A faithful FFT readout jitters every
/// frame, slopes off steeply toward the treble, and never sits still. All of the
/// shaping that fixes that lives here, in one place, shared by every bar layer
/// bound to it -- which also means the FFT runs once per provider rather than
/// once per bar layer.
///
/// It draws nothing; VisualizerCanvas has no delegate for it.
class AudioProviderLayer : public Layer
{
    Q_OBJECT

    // --- Resolution --------------------------------------------------------
    Q_PROPERTY(int bandCount MEMBER m_bandCount NOTIFY changed)
    /// Analysis window length. Longer resolves closely spaced partials but
    /// responds more slowly. Rounded up to a power of two internally.
    Q_PROPERTY(qreal windowMs MEMBER m_windowMs NOTIFY changed)
    Q_PROPERTY(qreal minHz MEMBER m_minHz NOTIFY changed)
    Q_PROPERTY(qreal maxHz MEMBER m_maxHz NOTIFY changed)
    /// 0 = logarithmic band spacing, 1 = linear. Linear over a narrow low range
    /// resolves a bass line's harmonics into separate spikes.
    Q_PROPERTY(qreal hzLinearFactor MEMBER m_hzLinearFactor NOTIFY changed)
    /// Overlapping windows averaged per frame. A single FFT is a high-variance
    /// estimate and the floor between partials looks like grass; averaging
    /// several cuts that noise and leaves clean peaks.
    Q_PROPERTY(int averageCount MEMBER m_averageCount NOTIFY changed)

    // --- Conditioning ------------------------------------------------------
    /// "rms" averages the bins in a band, "peak" takes the loudest. Peak follows
    /// individual partials and is the main cause of frame-to-frame jitter.
    Q_PROPERTY(QString bandMeasure MEMBER m_bandMeasure NOTIFY changed)
    /// "db" maps a floor..ceiling window onto 0..1; "linear" uses raw magnitude
    /// times linearGain. Linear leaves a flat floor with sharp peaks on it,
    /// which is the classic spectrum-analyser look; dB lifts everything and
    /// reads as one continuous mound.
    Q_PROPERTY(QString amplitudeMode MEMBER m_amplitudeMode NOTIFY changed)
    Q_PROPERTY(qreal linearGain MEMBER m_linearGain NOTIFY changed)
    /// Linear mode: black level subtracted after the gain, then rescaled. Quiet
    /// passages still carry rumble in every band; without this the display never
    /// rests at the baseline.
    Q_PROPERTY(qreal linearFloor MEMBER m_linearFloor NOTIFY changed)
    /// Gain at the bottom of the frequency range, ramping to 1.0 at the top.
    /// Bass dominates any mix; pulling the bottom down spreads the picture
    /// across the whole width instead of piling it up at the low end.
    Q_PROPERTY(qreal lowFreqMagnitude MEMBER m_lowFreqMagnitude NOTIFY changed)
    /// Lifts the treble to compensate music's natural roll-off, so the outer
    /// bars stay active instead of sitting on the floor.
    Q_PROPERTY(qreal tiltDbPerOctave MEMBER m_tiltDbPerOctave NOTIFY changed)
    /// Per-band auto-gain, 0..1. Each band is referenced to its own slow-moving
    /// AVERAGE level, so quiet bands stay as animated as loud ones.
    Q_PROPERTY(qreal bandNormalization MEMBER m_bandNormalization NOTIFY changed)
    Q_PROPERTY(qreal normalizationTau MEMBER m_normalizationTau NOTIFY changed)
    Q_PROPERTY(qreal floorDb MEMBER m_floorDb NOTIFY changed)
    Q_PROPERTY(qreal ceilingDb MEMBER m_ceilingDb NOTIFY changed)
    /// Soft gate width above the floor; larger means emptier gaps between hits.
    Q_PROPERTY(qreal gateDb MEMBER m_gateDb NOTIFY changed)
    /// "linear", "log" or "power"
    Q_PROPERTY(QString scalingCurve MEMBER m_scalingCurve NOTIFY changed)
    Q_PROPERTY(qreal gamma MEMBER m_gamma NOTIFY changed)

    // --- Motion ------------------------------------------------------------
    /// "attackDecay" rises fast and falls slow; "symmetric" uses one constant
    /// both ways, which avoids every bar falling at an identical mechanical rate.
    Q_PROPERTY(QString smoothingMode MEMBER m_smoothingMode NOTIFY changed)
    Q_PROPERTY(qreal smoothMs MEMBER m_smoothMs NOTIFY changed)
    Q_PROPERTY(qreal attackMs MEMBER m_attackMs NOTIFY changed)
    Q_PROPERTY(qreal decayMs MEMBER m_decayMs NOTIFY changed)
    /// Blends each band toward its neighbours, 0..1. Turns a comb of unrelated
    /// spikes into a single contour.
    Q_PROPERTY(qreal spatialSmoothing MEMBER m_spatialSmoothing NOTIFY changed)
    /// Minimum drawn level, so idle bars stay visible.
    Q_PROPERTY(qreal floorLevel MEMBER m_floorLevel NOTIFY changed)

    // --- Live data ---------------------------------------------------------
    Q_PROPERTY(QList<qreal> bands READ bands NOTIFY bandsChanged)
    Q_PROPERTY(qreal bass READ bass NOTIFY bandsChanged)
    Q_PROPERTY(qreal mid READ mid NOTIFY bandsChanged)
    Q_PROPERTY(qreal treble READ treble NOTIFY bandsChanged)

public:
    explicit AudioProviderLayer(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("audio"); }
    bool needsSpectrum() const override { return true; }
    bool isVisual() const override { return false; }

    SpectrumParams analysisParams() const override;
    void applyFrame(const SpectrumFrame &frame) override;

    const QList<qreal> &bands() const { return m_bands; }
    qreal bass() const { return m_bass; }
    qreal mid() const { return m_mid; }
    qreal treble() const { return m_treble; }
    const SpectrumFrame &frame() const { return m_frame; }

Q_SIGNALS:
    void bandsChanged();

private:
    int m_bandCount = 48;
    qreal m_windowMs = 85.0;
    qreal m_minHz = 40.0;
    qreal m_maxHz = 14000.0;
    qreal m_hzLinearFactor = 0.0;
    int m_averageCount = 1;

    QString m_bandMeasure = QStringLiteral("rms");
    QString m_amplitudeMode = QStringLiteral("db");
    qreal m_linearGain = 30.0;
    qreal m_linearFloor = 0.0;
    qreal m_lowFreqMagnitude = 1.0;
    qreal m_tiltDbPerOctave = 0.0;
    qreal m_bandNormalization = 0.7;
    qreal m_normalizationTau = 3.0;
    qreal m_floorDb = -78.0;
    qreal m_ceilingDb = -4.0;
    qreal m_gateDb = 8.0;
    QString m_scalingCurve = QStringLiteral("power");
    qreal m_gamma = 3.0;

    QString m_smoothingMode = QStringLiteral("attackDecay");
    qreal m_smoothMs = 60.0;
    qreal m_attackMs = 5.0;
    qreal m_decayMs = 220.0;
    qreal m_spatialSmoothing = 0.15;
    qreal m_floorLevel = 0.0;

    SpectrumFrame m_frame;
    QList<qreal> m_bands;
    qreal m_bass = 0.0;
    qreal m_mid = 0.0;
    qreal m_treble = 0.0;
};

/// Frequency-reactive EQ bars: the primary visualizer type.
///
/// Carries only how the bars LOOK. Everything about how the audio is turned into
/// numbers lives on the AudioProviderLayer this one points at.
class BarLayer : public Layer
{
    Q_OBJECT

    /// layerId of the AudioProviderLayer feeding this one; 0 means "the first
    /// provider in the stack", which is what a single-provider project wants.
    Q_PROPERTY(int providerId MEMBER m_providerId NOTIFY changed)

    // --- Shape -------------------------------------------------------------
    Q_PROPERTY(qreal barWidthRatio MEMBER m_barWidthRatio NOTIFY changed)
    Q_PROPERTY(qreal cornerRadius MEMBER m_cornerRadius NOTIFY changed)
    Q_PROPERTY(qreal minHeight MEMBER m_minHeight NOTIFY changed)
    Q_PROPERTY(qreal maxHeight MEMBER m_maxHeight NOTIFY changed)

    // --- Placement ---------------------------------------------------------
    /// "linear", "mirrored" or "radial". Note this mirrors VERTICALLY (bars grow
    /// both up and down); to mirror the frequency axis use `mirrorSpectrum`.
    Q_PROPERTY(QString orientation MEMBER m_orientation NOTIFY changed)
    /// Reflects the FREQUENCY axis: lowest band at both outer edges, highest
    /// meeting in the centre. This is the symmetric skyline look, and is
    /// independent of `orientation`.
    Q_PROPERTY(bool mirrorSpectrum MEMBER m_mirrorSpectrum NOTIFY changed)
    /// Radial only: hole size as a fraction of the layer's smaller dimension.
    Q_PROPERTY(qreal innerRadius MEMBER m_innerRadius NOTIFY changed)
    Q_PROPERTY(bool growUpward MEMBER m_growUpward NOTIFY changed)

    // --- Colour ------------------------------------------------------------
    /// "solid", "gradient" or "spectrum" (hue swept across the bars)
    Q_PROPERTY(QString colorMode MEMBER m_colorMode NOTIFY changed)
    Q_PROPERTY(QColor colorA MEMBER m_colorA NOTIFY changed)
    Q_PROPERTY(QColor colorB MEMBER m_colorB NOTIFY changed)
    /// Extra brightness applied in proportion to a bar's level, 0 = off.
    Q_PROPERTY(qreal reactiveBrightness MEMBER m_reactiveBrightness NOTIFY changed)

    // --- Live data, copied from the provider each frame ---------------------
    /// Already reflected when `mirrorSpectrum` is set, so the view just draws it.
    Q_PROPERTY(QList<qreal> bands READ bands NOTIFY bandsChanged)
    Q_PROPERTY(int barCount READ barCount NOTIFY bandsChanged)
    Q_PROPERTY(qreal bass READ bass NOTIFY bandsChanged)
    Q_PROPERTY(qreal mid READ mid NOTIFY bandsChanged)
    Q_PROPERTY(qreal treble READ treble NOTIFY bandsChanged)

public:
    explicit BarLayer(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("bars"); }

    const QList<qreal> &bands() const { return m_bands; }
    int barCount() const { return int(m_bands.size()); }
    qreal bass() const { return m_bass; }
    qreal mid() const { return m_mid; }
    qreal treble() const { return m_treble; }

    int providerId() const { return m_providerId; }
    bool mirrorSpectrum() const { return m_mirrorSpectrum; }

    /// Take a provider's frame, applying spectrum mirroring if enabled.
    void takeFrame(const SpectrumFrame &frame);

Q_SIGNALS:
    void bandsChanged();

private:
    int m_providerId = 0;
    bool m_mirrorSpectrum = false;
    qreal m_barWidthRatio = 0.65;
    qreal m_cornerRadius = 2.0;
    qreal m_minHeight = 0.005;
    qreal m_maxHeight = 0.8;

    QString m_orientation = QStringLiteral("linear");
    qreal m_innerRadius = 0.35;
    bool m_growUpward = true;

    QString m_colorMode = QStringLiteral("gradient");
    QColor m_colorA = QColor(QStringLiteral("#4cc9f0"));
    QColor m_colorB = QColor(QStringLiteral("#f72585"));
    qreal m_reactiveBrightness = 0.0;

    QList<qreal> m_bands;
    qreal m_bass = 0.0;
    qreal m_mid = 0.0;
    qreal m_treble = 0.0;
};

/// Flat colour or image behind everything else.
class BackgroundLayer : public Layer
{
    Q_OBJECT

    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    Q_PROPERTY(QString imageSource MEMBER m_imageSource NOTIFY changed)
    /// "stretch", "fit" or "crop"
    Q_PROPERTY(QString fillMode MEMBER m_fillMode NOTIFY changed)

public:
    explicit BackgroundLayer(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("background"); }

private:
    QColor m_color = QColor(QStringLiteral("#0b0b12"));
    QString m_imageSource;
    QString m_fillMode = QStringLiteral("crop");
};

/// Particle burst driven by one frequency band.
///
/// Carries its own analyzer settings (see Layer::analysisParams) so its
/// reactivity is independent of any bar layer on the canvas.
class ParticleLayer : public Layer
{
    Q_OBJECT

    /// Which energy drives emission: "bass", "mid" or "treble".
    Q_PROPERTY(QString band MEMBER m_band NOTIFY changed)
    /// Particles per second when the band is silent.
    Q_PROPERTY(qreal baseRate MEMBER m_baseRate NOTIFY changed)
    /// Extra particles per second at full band energy.
    Q_PROPERTY(qreal reactiveRate MEMBER m_reactiveRate NOTIFY changed)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    Q_PROPERTY(qreal particleSize MEMBER m_particleSize NOTIFY changed)
    Q_PROPERTY(qreal lifeSpan MEMBER m_lifeSpan NOTIFY changed)
    Q_PROPERTY(qreal speed MEMBER m_speed NOTIFY changed)
    /// Emission direction in degrees; 270 is straight up.
    Q_PROPERTY(qreal angle MEMBER m_angle NOTIFY changed)
    Q_PROPERTY(qreal angleSpread MEMBER m_angleSpread NOTIFY changed)
    Q_PROPERTY(qreal gravity MEMBER m_gravity NOTIFY changed)

    Q_PROPERTY(qreal level READ level NOTIFY levelChanged)

public:
    explicit ParticleLayer(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("particles"); }
    bool needsSpectrum() const override { return true; }
    SpectrumParams analysisParams() const override;
    void applyFrame(const SpectrumFrame &frame) override;

    /// Energy of the selected band, 0..1.
    qreal level() const { return m_level; }

Q_SIGNALS:
    void levelChanged();

private:
    QString m_band = QStringLiteral("bass");
    qreal m_baseRate = 0.0;
    qreal m_reactiveRate = 240.0;
    QColor m_color = QColor(QStringLiteral("#4cc9f0"));
    qreal m_particleSize = 14.0;
    qreal m_lifeSpan = 1600.0;
    qreal m_speed = 220.0;
    qreal m_angle = 270.0;
    qreal m_angleSpread = 40.0;
    qreal m_gravity = 90.0;

    qreal m_level = 0.0;
};

/// Static text, e.g. the track title.
class TextLayer : public Layer
{
    Q_OBJECT

    Q_PROPERTY(QString text MEMBER m_text NOTIFY changed)
    Q_PROPERTY(QString fontFamily MEMBER m_fontFamily NOTIFY changed)
    /// Point size as a fraction of canvas height, so text scales with resolution.
    Q_PROPERTY(qreal fontScale MEMBER m_fontScale NOTIFY changed)
    Q_PROPERTY(bool bold MEMBER m_bold NOTIFY changed)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    /// "left", "center" or "right"
    Q_PROPERTY(QString alignment MEMBER m_alignment NOTIFY changed)

public:
    explicit TextLayer(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("text"); }

private:
    QString m_text = QStringLiteral("Track title");
    QString m_fontFamily = QStringLiteral("Segoe UI");
    qreal m_fontScale = 0.06;
    bool m_bold = true;
    QColor m_color = QColor(QStringLiteral("#ffffff"));
    QString m_alignment = QStringLiteral("center");
};

} // namespace mv
