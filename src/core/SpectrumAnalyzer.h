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

struct SpectrumParams
{
    int fftSize = 2048;
    int bandCount = 64;
    double minHz = 30.0;
    double maxHz = 16000.0;
    double floorDb = -72.0;
    double ceilingDb = -6.0;
    ScalingCurve curve = ScalingCurve::Linear;
    double gamma = 1.0;

    /// Smoothing expressed as TIME CONSTANTS in seconds, deliberately not as
    /// per-frame factors. A per-frame factor would make a 30 fps export decay at
    /// half the rate of a 60 Hz preview and the two would visibly disagree.
    double attackTau = 0.010;
    double decayTau = 0.180;
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

    SpectrumParams m_params;
    std::unique_ptr<Fft> m_fft;

    QVector<float> m_window;   ///< Hann coefficients
    QVector<float> m_frame;    ///< windowed mono samples
    QVector<float> m_scratchRe;
    QVector<float> m_scratchIm;
    QVector<float> m_magnitudes;

    QVector<int> m_bandStart;  ///< first FFT bin of each band
    QVector<int> m_bandEnd;    ///< one past the last bin of each band
    QVector<float> m_smoothed;

    int m_builtForRate = 0;
    bool m_dirty = true;
};

} // namespace mv
