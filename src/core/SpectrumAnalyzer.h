#pragma once

#include "AudioBuffer.h"
#include "Fft.h"

#include <QVector>
#include <memory>

namespace mv {

/// How band magnitudes are shaped before they reach the visualizer.
enum class ScalingCurve
{
    Linear,      ///< normalized dB, straight through
    Logarithmic, ///< emphasises quiet detail
    Power        ///< pow(v, gamma); gamma > 1 tightens, < 1 lifts
};

/// How the bins inside one band are collapsed to a single value.
enum class BandMeasure
{
    Peak, ///< loudest bin. Tracks partials tightly but jitters frame to frame.
    Rms   ///< energy average. Much steadier, and what looks good on screen.
};

/// How band magnitude becomes a 0..1 level.
enum class AmplitudeMode
{
    /// Normalized decibels between a floor and a ceiling. Perceptual, and it
    /// keeps every band visible -- but it lifts the noise between partials just
    /// as much as the partials, so the row reads as one continuous mound.
    Decibel,
    /// Raw magnitude times a gain. Quiet content stays near zero, so individual
    /// partials stand out as sharp spikes over a flat floor. This is what
    /// Avee-style visualizers do and it is the difference between "a mound" and
    /// "a row of peaks".
    Linear
};

/// How the temporal envelope follows the signal.
enum class SmoothingMode
{
    /// Separate rise and fall time constants.
    AttackDecay,
    /// One time constant in both directions. Reads as more organic: an
    /// asymmetric envelope makes every bar fall at the same rate regardless of
    /// the music, which looks mechanical.
    Symmetric
};

/// Everything that turns raw audio into numbers a bar layer can draw.
///
/// These are deliberately "how it should look" controls rather than a faithful
/// analyser: a straight FFT readout is spiky, slopes off steeply toward the
/// treble, and never settles. See AudioProviderLayer, which owns a set of these
/// and shares its output with every bar layer bound to it.
struct SpectrumParams
{
    /// True analysis window, in samples. This alone sets frequency resolution:
    /// a pure tone spreads over roughly 2 * sampleRate / windowSamples Hz.
    ///
    /// Deliberately NOT constrained to a power of two. The transform is zero
    /// padded up to one internally, so any window length is available and
    /// resolution can be dialled continuously. Rounding the window itself to a
    /// power of two would quantise resolution in factor-of-two jumps -- 4096
    /// samples gives ~23 Hz and 8192 gives ~12 Hz, with nothing in between.
    int windowSamples = 4096;

    /// Zero-padding factor. Padding does not improve resolution, but it places
    /// more bins under the same peak, which lets the per-band interpolation
    /// locate it smoothly instead of in bin-sized steps.
    int padFactor = 2;

    int bandCount = 48;
    double minHz = 40.0;
    double maxHz = 14000.0;

    /// 0 = logarithmic band spacing (equal width per octave), 1 = linear
    /// (equal width per Hz). Linear over a narrow low range is what resolves a
    /// bass line's individual harmonics into separate spikes; logarithmic over a
    /// wide range smears them together.
    double hzLinearFactor = 0.0;

    /// How many overlapping windows are averaged per frame (Welch's method).
    ///
    /// A single FFT is a high-variance estimate: the noise between partials
    /// jumps around from frame to frame and the display looks like grass.
    /// Averaging several half-overlapped windows cuts that variance roughly by
    /// this factor, which is what leaves a flat floor with clean peaks standing
    /// on it. Costs one extra transform each.
    int averageCount = 1;

    /// Bins inside a band are averaged by default. Peak picks up any stray
    /// partial or noise spike and is the single biggest source of jitter.
    BandMeasure measure = BandMeasure::Rms;

    /// Music falls off roughly 3-6 dB per octave, so an honest spectrum leaves
    /// the treble bars stuck near the floor. Tilting the response back up keeps
    /// the whole width visually alive.
    double tiltDbPerOctave = 0.0;

    /// Per-band auto-gain, 0..1. Each band is referenced to its own slow-moving
    /// AVERAGE level, so a quiet band is as animated as a loud one. A fixed tilt
    /// can only correct the average slope; this also handles a track whose
    /// energy is unevenly spread, which is what stops the outer bars from merely
    /// twitching while the middle does all the work.
    ///
    /// Referenced to the average and not to the recent peak on purpose: a peak
    /// reference pins every band at its own maximum and flattens the dynamics
    /// completely, which measured as a punch ratio of exactly 1.0.
    double bandNormalization = 0.7;

    /// Seconds for a band's reference level to follow a change. Long enough to
    /// ride over individual beats, short enough to track a new section.
    double normalizationTau = 3.0;

    AmplitudeMode amplitude = AmplitudeMode::Decibel;

    /// Linear mode only: magnitude is multiplied by this to reach 0..1.
    double linearGain = 30.0;

    /// Gain applied at the very bottom of the frequency range, ramping linearly
    /// to 1.0 at the top. 1.0 leaves the response alone.
    ///
    /// Bass dominates almost any mix, so without this the lowest bands tower
    /// over everything and the rest of the display is a footnote. Pulling the
    /// bottom down redistributes the picture across the whole width. This is a
    /// straight ramp across BAND INDEX, unlike tiltDbPerOctave which works in
    /// octaves -- over a narrow linear range there is barely an octave to work
    /// with and a per-octave tilt has almost no effect.
    double lowFreqMagnitude = 1.0;

    // Decibel mode only.
    double floorDb = -78.0;
    double ceilingDb = -4.0;

    /// Pulls anything near the floor down to nothing, so the gaps between beats
    /// read as genuinely empty instead of a low simmer.
    double gateDb = 8.0;

    ScalingCurve curve = ScalingCurve::Power;
    double gamma = 3.0;

    /// Smoothing expressed as TIME CONSTANTS in seconds, deliberately not as
    /// per-frame factors. A per-frame factor would make a 30 fps export decay at
    /// half the rate of a 60 Hz preview and the two would visibly disagree.
    SmoothingMode smoothing = SmoothingMode::AttackDecay;
    double attackTau = 0.005;
    double decayTau = 0.220;
    /// Symmetric mode only: one time constant for both directions.
    double smoothTau = 0.060;

    /// Blends each band toward its neighbours, 0..1. Turns a comb of unrelated
    /// spikes into a contour that reads as one shape.
    double spatialSmoothing = 0.15;

    /// Minimum drawn level, so idle bars stay visible instead of vanishing.
    double floorLevel = 0.0;
};

struct SpectrumFrame
{
    QVector<float> bands; ///< bandCount values, each 0..1
    float bass = 0.0f;    ///< aggregate energies for reactive layers, 0..1
    float mid = 0.0f;
    float treble = 0.0f;
};

/// Turns (audio buffer, timestamp) into normalized frequency bands.
///
/// The transform itself is a pure function of the inputs; the only state carried
/// between calls is the smoothing envelope. Each consumer (live preview, export)
/// owns its own analyzer, so their envelopes evolve independently and neither can
/// perturb the other.
class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer();

    void setParams(const SpectrumParams &params);
    const SpectrumParams &params() const { return m_params; }

    /// Analyze the window centred on `timeSeconds`, advancing the smoothing
    /// envelope by `dt` seconds. Pass the real frame interval: 1/60 for preview,
    /// 1/fps for export.
    SpectrumFrame analyze(const AudioBuffer &buffer, double timeSeconds, double dt);

    /// Drop the smoothing envelope, e.g. after a seek or before an export run.
    void reset();

private:
    void rebuild();
    void layoutBands(int sampleRate);

    SpectrumParams m_params;
    std::unique_ptr<Fft> m_fft;
    int m_fftSize = 0;      ///< padded transform size, a power of two
    int m_windowLen = 0;    ///< true window length inside it

    QVector<float> m_window;   ///< Hann coefficients
    QVector<float> m_frame;    ///< windowed mono samples
    QVector<float> m_scratchRe;
    QVector<float> m_scratchIm;
    QVector<float> m_magnitudes;
    QVector<float> m_power;    ///< accumulator for Welch averaging

    // Fractional bin edges. Integer edges would make neighbouring bands cover
    // different whole numbers of bins whenever the bands are comparable in width
    // to a bin, stamping a fixed comb pattern across the display.
    QVector<float> m_bandLo;    ///< first bin of each band, fractional
    QVector<float> m_bandHi;    ///< last bin of each band, fractional
    QVector<float> m_bandGainDb;///< per-band tilt, precomputed
    QVector<float> m_centreHz;
    QVector<float> m_raw;       ///< pre-smoothing values, reused each frame
    QVector<float> m_smoothed;
    QVector<float> m_bandRefDb; ///< per-band reference peak for auto-gain

    int m_builtForRate = 0;
    bool m_dirty = true;
};

} // namespace mv
