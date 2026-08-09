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

/// Everything that turns raw audio into numbers a bar layer can draw.
///
/// These are deliberately "how it should look" controls rather than a faithful
/// analyser: a straight FFT readout is spiky, slopes off steeply toward the
/// treble, and never settles. See AudioProviderLayer, which owns a set of these
/// and shares its output with every bar layer bound to it.
struct SpectrumParams
{
    int fftSize = 4096;
    int bandCount = 48;
    double minHz = 40.0;
    double maxHz = 14000.0;

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
    double attackTau = 0.005;
    double decayTau = 0.220;

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

    QVector<float> m_window;   ///< Hann coefficients
    QVector<float> m_frame;    ///< windowed mono samples
    QVector<float> m_scratchRe;
    QVector<float> m_scratchIm;
    QVector<float> m_magnitudes;

    QVector<int> m_bandStart;   ///< first FFT bin of each band
    QVector<int> m_bandEnd;     ///< one past the last bin of each band
    QVector<float> m_bandGainDb;///< per-band tilt, precomputed
    QVector<float> m_centreHz;
    QVector<float> m_raw;       ///< pre-smoothing values, reused each frame
    QVector<float> m_smoothed;
    QVector<float> m_bandRefDb; ///< per-band reference peak for auto-gain

    int m_builtForRate = 0;
    bool m_dirty = true;
};

} // namespace mv
