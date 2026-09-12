#pragma once

#include "NodePorts.h"

#include <SpectrumAnalyzer.h>

#include <QColor>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVector>

namespace mv {

class NodeGraph;

/// Base class for everything in the node graph.
///
/// Properties are declared with Q_PROPERTY(... MEMBER ...) on purpose: it gives
/// QML two-way bindings and lets Project.cpp serialize by walking the meta-object,
/// so adding a property here costs no serialization code and no panel plumbing.
///
/// They all share one `changed` NOTIFY signal. QML re-evaluates the affected
/// bindings on it, and the slight over-invalidation is far cheaper than
/// hand-writing a signal per property.
///
/// ------------------------------------------------------------------------
/// An AUDIO EDGE SELECTS A SOURCE. IT DOES NOT CARRY NUMBERS.
/// ------------------------------------------------------------------------
///
/// This is the one thing about the graph that is not guessable from its shape. A
/// SeparatorNode physically needs PCM to run an FFT over, not its input's band
/// values -- so its audio input names WHICH audio to analyse, and
/// AudioSourceNode::audioRoot() walks that chain back to the AudioNode holding
/// the samples. A separator never reads its immediate input's bands.
///
/// Two corollaries:
///
///   - Chained separators do NOT cascade. Two in series produce two independent
///     analyses of the same PCM, each with its own band count and smoothing.
///     That is allowed -- forbidding it would be a special case in an otherwise
///     uniform type system -- but it is not a two-stage filter, and nobody
///     should expect the second to refine the first.
///
///   - The AudioNode is itself just an analyzer, with bandCount 1 over the full
///     range. "One bar = overall volume" therefore falls out of the type system
///     rather than needing a node of its own.
class Node : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString typeId READ typeId CONSTANT)
    Q_PROPERTY(int nodeId READ nodeId NOTIFY nodeIdChanged)

    Q_PROPERTY(QString name MEMBER m_name NOTIFY changed)

    /// Position in graph space. MEMBER so the drag handler writes it directly and
    /// Project.cpp's meta-walk persists it with no extra code -- the same bargain
    /// the rest of the property set already makes.
    Q_PROPERTY(qreal graphX MEMBER m_graphX NOTIFY changed)
    Q_PROPERTY(qreal graphY MEMBER m_graphY NOTIFY changed)

public:
    explicit Node(QObject *parent = nullptr);

    virtual QString typeId() const = 0;

    /// This type's port table entry. Never null: a type with no spec cannot be
    /// constructed, and tst_graph pins that the two lists agree.
    const NodeTypeSpec &spec() const;

    int nodeId() const { return m_nodeId; }

    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; Q_EMIT changed(); }

    /// True when this node has a delegate in VisualizerCanvas. Derived from the
    /// port table rather than overridden per class -- the table is the one place
    /// that decides, because the flatten and the editor read it too.
    bool isVisual() const { return spec().visual; }

    int inputCount() const;
    Node *input(int port) const;
    Node *input(const QString &portName) const;
    /// Index of `portName` among this type's inputs, or -1.
    int portIndex(const QString &portName) const;

    /// nodeId of whatever feeds `portName`, or 0 if nothing does.
    Q_INVOKABLE int sourceId(const QString &portName) const;

Q_SIGNALS:
    void changed();
    void nodeIdChanged();

protected:
    QString m_name;
    qreal m_graphX = 0.0;
    qreal m_graphY = 0.0;

private:
    // Only the graph may rewire. In memory an input is a SLOT, so "one input, one
    // source" is structurally impossible to violate -- there is nowhere to put a
    // second source, the deletion cascade is one pass nulling matching pointers,
    // and the flatten is a pointer walk with no lookups. QML and the inspector
    // cannot reach these at all, so they cannot construct an illegal state.
    //
    // On disk it is a flat edge list keyed by port NAME instead, which is
    // readable, diffable, and survives the port table being reordered.
    friend class NodeGraph;
    void setNodeId(int id);
    void setInput(int port, Node *source);

    /// Sized from spec().inputs, lazily: spec() is virtual and so cannot be read
    /// from this constructor.
    QVector<Node *> &slots_() const;

    mutable QVector<Node *> m_inputs;
    int m_nodeId = 0;
};

/// A node that draws. Placement, compositing and visibility live here rather than
/// on Node, because a separator has no position and an output has no opacity --
/// and when they did, the inspector showed seven dead controls for them.
class VisualNode : public Node
{
    Q_OBJECT

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
    explicit VisualNode(QObject *parent = nullptr);

    bool layerVisible() const { return m_layerVisible; }
    qreal layerOpacity() const { return m_layerOpacity; }

protected:
    bool m_layerVisible = true;
    qreal m_layerOpacity = 1.0;
    QString m_blendMode = QStringLiteral("normal");
    qreal m_posX = 0.5;
    qreal m_posY = 0.5;
    qreal m_sizeW = 1.0;
    qreal m_sizeH = 1.0;
    qreal m_layerRotation = 0.0;
};

/// A node that owns a SpectrumAnalyzer and produces a frame each tick.
///
/// SpectrumEngine schedules one analysis per live source, so the FFT cost scales
/// with the number of sources rather than with the number of things drawing them.
class AudioSourceNode : public Node
{
    Q_OBJECT

    Q_PROPERTY(QList<qreal> bands READ bands NOTIFY bandsChanged)

public:
    explicit AudioSourceNode(QObject *parent = nullptr);

    /// Analyzer settings for this node. Each source gets its own analyzer, so
    /// band counts and smoothing are genuinely per-node.
    virtual SpectrumParams analysisParams() const = 0;

    /// Receive this frame's analysis. Called on the GUI thread.
    virtual void applyFrame(const SpectrumFrame &frame);

    const SpectrumFrame &frame() const { return m_frame; }
    const QList<qreal> &bands() const { return m_bands; }

    /// Walk the audio-input chain to the node that owns the PCM.
    ///
    /// Nullptr means "not plugged into anything", which is how the engine knows
    /// to skip this source entirely. An AudioNode is its own root.
    const Node *audioRoot() const;

Q_SIGNALS:
    void bandsChanged();

protected:
    SpectrumFrame m_frame;
    QList<qreal> m_bands;
};

/// The track itself, as a one-band analysis over the full frequency range.
///
/// Deliberately exposes `level` and NOT bass/mid/treble: with a single band the
/// centre frequency lands in exactly one of those three buckets and the other two
/// read 0.0, which would be a trap rather than an API.
class AudioNode : public AudioSourceNode
{
    Q_OBJECT

    Q_PROPERTY(qreal level READ level NOTIFY bandsChanged)

public:
    explicit AudioNode(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("audio"); }

    SpectrumParams analysisParams() const override;
    void applyFrame(const SpectrumFrame &frame) override;

    /// Overall loudness, 0..1.
    qreal level() const { return m_level; }

private:
    qreal m_level = 0.0;
};

/// Conditions the audio into the numbers the spectrum nodes actually draw.
///
/// This exists as its own node because "what the spectrum really is" and "what
/// looks good" are different problems. A faithful FFT readout jitters every
/// frame, slopes off steeply toward the treble, and never sits still. All of the
/// shaping that fixes that lives here, in one place, shared by every spectrum
/// node wired to it -- which also means the FFT runs once per separator rather
/// than once per spectrum.
///
/// It draws nothing; VisualizerCanvas has no delegate for it.
class SeparatorNode : public AudioSourceNode
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
    Q_PROPERTY(qreal bass READ bass NOTIFY bandsChanged)
    Q_PROPERTY(qreal mid READ mid NOTIFY bandsChanged)
    Q_PROPERTY(qreal treble READ treble NOTIFY bandsChanged)

public:
    explicit SeparatorNode(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("separator"); }

    SpectrumParams analysisParams() const override;
    void applyFrame(const SpectrumFrame &frame) override;

    qreal bass() const { return m_bass; }
    qreal mid() const { return m_mid; }
    qreal treble() const { return m_treble; }

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

    qreal m_bass = 0.0;
    qreal m_mid = 0.0;
    qreal m_treble = 0.0;
};

/// Frequency-reactive EQ bars: the primary visualizer type.
///
/// Carries only how the bars LOOK. Everything about how the audio is turned into
/// numbers lives on the separator wired to its audio input.
class SpectrumNode : public VisualNode
{
    Q_OBJECT

    // --- Shape -------------------------------------------------------------
    Q_PROPERTY(qreal barWidthRatio MEMBER m_barWidthRatio NOTIFY changed)
    Q_PROPERTY(qreal cornerRadius MEMBER m_cornerRadius NOTIFY changed)
    Q_PROPERTY(qreal minHeight MEMBER m_minHeight NOTIFY changed)
    Q_PROPERTY(qreal maxHeight MEMBER m_maxHeight NOTIFY changed)

    // --- Placement ---------------------------------------------------------
    /// "linear" or "radial".
    Q_PROPERTY(QString orientation MEMBER m_orientation NOTIFY changed)
    /// Two-sided bars: each one grows both ways from its base line rather than
    /// only into the layer. Works in either orientation -- linear grows up and
    /// down from the centre line, radial grows out and in from the inner ring.
    /// This is a VERTICAL mirror; to mirror the frequency axis use
    /// `mirrorSpectrum`.
    Q_PROPERTY(bool mirrorBars MEMBER m_mirrorBars NOTIFY changed)
    /// Reflects the FREQUENCY axis: the band order is repeated back on itself so
    /// the spectrum reads symmetrically. This is the symmetric skyline look, and
    /// is independent of `orientation`.
    Q_PROPERTY(bool mirrorSpectrum MEMBER m_mirrorSpectrum NOTIFY changed)
    /// Which end of the mirrored spectrum meets in the centre: false puts the
    /// lowest band at both outer edges (treble meeting in the middle), true
    /// flips it. Only meaningful with `mirrorSpectrum`.
    Q_PROPERTY(bool bassInner MEMBER m_bassInner NOTIFY changed)
    /// Radial only: hole size as a fraction of the layer's smaller dimension.
    Q_PROPERTY(qreal innerRadius MEMBER m_innerRadius NOTIFY changed)
    /// Linear only: bends the base line into a circular arc subtending
    /// `|arc| * 180` degrees. 0 is a straight line, positive bows the middle up
    /// (a dome), negative bows it down (a bowl).
    Q_PROPERTY(qreal arc MEMBER m_arc NOTIFY changed)
    /// Bars taper to fill their angular slot at every radius instead of staying
    /// constant-width, so an arced spectrum has no wedge gaps. Only meaningful
    /// with `arc`.
    Q_PROPERTY(bool barsMatchArc MEMBER m_barsMatchArc NOTIFY changed)
    Q_PROPERTY(bool growUpward MEMBER m_growUpward NOTIFY changed)

    // --- Colour ------------------------------------------------------------
    /// "solid", "gradient" or "spectrum" (hue swept across the bars)
    Q_PROPERTY(QString colorMode MEMBER m_colorMode NOTIFY changed)
    Q_PROPERTY(QColor colorA MEMBER m_colorA NOTIFY changed)
    Q_PROPERTY(QColor colorB MEMBER m_colorB NOTIFY changed)
    /// Extra brightness applied in proportion to a bar's level, 0 = off.
    Q_PROPERTY(qreal reactiveBrightness MEMBER m_reactiveBrightness NOTIFY changed)

    // --- Live data, copied from the separator each frame --------------------
    /// Already reflected when `mirrorSpectrum` is set, so the view just draws it.
    Q_PROPERTY(QList<qreal> bands READ bands NOTIFY bandsChanged)
    Q_PROPERTY(int barCount READ barCount NOTIFY bandsChanged)
    /// Bars at each outer edge that carry no distinct content and should not be
    /// drawn. They come from bands below the analysis window's lowest resolvable
    /// frequency, which all collapse onto the same bin; drawn, they merge into
    /// one wide slab.
    Q_PROPERTY(int inactiveEdgeBars READ inactiveEdgeBars NOTIFY bandsChanged)
    Q_PROPERTY(qreal bass READ bass NOTIFY bandsChanged)
    Q_PROPERTY(qreal mid READ mid NOTIFY bandsChanged)
    Q_PROPERTY(qreal treble READ treble NOTIFY bandsChanged)

public:
    explicit SpectrumNode(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("spectrum"); }

    const QList<qreal> &bands() const { return m_bands; }
    int barCount() const { return int(m_bands.size()); }
    int inactiveEdgeBars() const { return m_inactiveEdgeBars; }
    qreal bass() const { return m_bass; }
    qreal mid() const { return m_mid; }
    qreal treble() const { return m_treble; }

    bool mirrorSpectrum() const { return m_mirrorSpectrum; }

    /// Take a separator's frame, applying spectrum mirroring if enabled.
    void takeFrame(const SpectrumFrame &frame);

    /// Drop to nothing, for an unconnected audio input. Wiring is explicit and
    /// visible now, so an unconnected spectrum draws nothing rather than quietly
    /// borrowing whichever separator happened to come first.
    void clearFrame();

Q_SIGNALS:
    void bandsChanged();

private:
    int m_inactiveEdgeBars = 0;
    bool m_mirrorSpectrum = false;
    bool m_bassInner = false;
    qreal m_barWidthRatio = 0.65;
    qreal m_cornerRadius = 2.0;
    qreal m_minHeight = 0.005;
    qreal m_maxHeight = 0.8;

    QString m_orientation = QStringLiteral("linear");
    bool m_mirrorBars = false;
    qreal m_innerRadius = 0.35;
    qreal m_arc = 0.0;
    bool m_barsMatchArc = false;
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

/// Flat colour behind everything else. An Image node comes later.
class BackgroundNode : public VisualNode
{
    Q_OBJECT

    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)

public:
    explicit BackgroundNode(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("background"); }

private:
    QColor m_color = QColor(QStringLiteral("#0b0b12"));
};

/// Static text, e.g. the track title.
class TextNode : public VisualNode
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
    explicit TextNode(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("text"); }

private:
    QString m_text = QStringLiteral("Track title");
    QString m_fontFamily = QStringLiteral("Segoe UI");
    qreal m_fontScale = 0.06;
    bool m_bold = true;
    QColor m_color = QColor(QStringLiteral("#ffffff"));
    QString m_alignment = QStringLiteral("center");
};

/// The root of the render. Exactly one exists and it cannot be deleted.
///
/// Walking its visual input backwards produces the draw order, which is the whole
/// reason layering needs no array index any more. It is NOT a VisualNode, so it
/// never enters the render list itself.
class OutputNode : public Node
{
    Q_OBJECT

public:
    explicit OutputNode(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("output"); }
};

} // namespace mv
