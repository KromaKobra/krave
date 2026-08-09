#include "SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace mv {
namespace {

constexpr double kPi = 3.14159265358979323846;

int nextPowerOfTwo(int n)
{
    int p = 2;
    while (p < n)
        p <<= 1;
    return p;
}

/// Exponential blend factor for a given time constant and frame interval.
/// Deriving it from dt is what keeps a 30 fps export looking like a 60 Hz preview.
float blendFactor(double tau, double dt)
{
    if (tau <= 0.0)
        return 1.0f;
    return 1.0f - float(std::exp(-dt / tau));
}

float amplitudeToNormalizedDb(float amplitude, double floorDb, double ceilingDb)
{
    const double db = 20.0 * std::log10(double(amplitude) + 1e-12);
    const double span = ceilingDb - floorDb;
    if (span <= 0.0)
        return 0.0f;
    return float(std::clamp((db - floorDb) / span, 0.0, 1.0));
}

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer()
{
    rebuild();
}

void SpectrumAnalyzer::setParams(const SpectrumParams &params)
{
    m_params = params;
    m_params.fftSize = std::clamp(nextPowerOfTwo(std::max(64, m_params.fftSize)), 64, 16384);
    m_params.bandCount = std::clamp(m_params.bandCount, 1, 512);
    m_params.minHz = std::max(1.0, m_params.minHz);
    m_params.maxHz = std::max(m_params.minHz * 1.01, m_params.maxHz);
    m_dirty = true;
}

void SpectrumAnalyzer::reset()
{
    m_smoothed.fill(0.0f);
}

void SpectrumAnalyzer::rebuild()
{
    const int n = m_params.fftSize;

    m_fft = std::make_unique<Fft>(n);

    m_window.resize(n);
    for (int i = 0; i < n; ++i)
        m_window[i] = float(0.5 * (1.0 - std::cos(2.0 * kPi * double(i) / double(n - 1))));

    m_frame.resize(n);
    m_scratchRe.resize(n);
    m_scratchIm.resize(n);
    m_magnitudes.resize(n / 2);

    m_smoothed.assign(m_params.bandCount, 0.0f);
    m_bandStart.resize(m_params.bandCount);
    m_bandEnd.resize(m_params.bandCount);

    m_dirty = false;
}

SpectrumFrame SpectrumAnalyzer::analyze(const AudioBuffer &buffer, double timeSeconds, double dt)
{
    const int sampleRate = buffer.sampleRate() > 0 ? buffer.sampleRate() : 48000;

    if (m_dirty || sampleRate != m_builtForRate) {
        if (m_dirty)
            rebuild();

        // Band edges depend on the sample rate, so they are laid out here rather
        // than in rebuild(). Spacing is logarithmic: octaves get equal screen
        // width, which is how people actually hear pitch.
        const int n = m_params.fftSize;
        const int bins = n / 2;
        const double binsPerHz = double(n) / double(sampleRate);
        const double logMin = std::log(m_params.minHz);
        const double logMax = std::log(m_params.maxHz);
        const int count = m_params.bandCount;

        for (int b = 0; b < count; ++b) {
            const double f0 = std::exp(logMin + (logMax - logMin) * double(b) / double(count));
            const double f1 = std::exp(logMin + (logMax - logMin) * double(b + 1) / double(count));

            int start = int(std::floor(f0 * binsPerHz));
            int end = int(std::ceil(f1 * binsPerHz));

            start = std::clamp(start, 1, bins - 1); // bin 0 is DC
            end = std::clamp(end, start + 1, bins);

            m_bandStart[b] = start;
            m_bandEnd[b] = end;
        }

        m_builtForRate = sampleRate;
    }

    const int n = m_params.fftSize;
    const int count = m_params.bandCount;

    SpectrumFrame out;
    out.bands.resize(count);

    if (buffer.isEmpty()) {
        // Still decay toward silence so the bars fall away instead of freezing.
        const float k = blendFactor(m_params.decayTau, dt);
        for (int b = 0; b < count; ++b) {
            m_smoothed[b] += (0.0f - m_smoothed[b]) * k;
            out.bands[b] = m_smoothed[b];
        }
        return out;
    }

    // Centre the window on the requested timestamp; readMono zero-pads the edges.
    const qint64 centre = buffer.frameForTime(timeSeconds);
    buffer.readMono(centre - n / 2, m_frame.data(), n);
    for (int i = 0; i < n; ++i)
        m_frame[i] *= m_window[i];

    m_fft->magnitudes(m_frame.constData(), m_magnitudes.data(),
                      m_scratchRe.data(), m_scratchIm.data());

    // Normalize for window length and the Hann window's coherent gain, so the
    // dB scale means the same thing regardless of fftSize.
    const float norm = 2.0f / (float(n) * 0.5f);

    const float attack = blendFactor(m_params.attackTau, dt);
    const float decay = blendFactor(m_params.decayTau, dt);

    for (int b = 0; b < count; ++b) {
        float peak = 0.0f;
        for (int i = m_bandStart[b]; i < m_bandEnd[b]; ++i)
            peak = std::max(peak, m_magnitudes[i]);

        float v = amplitudeToNormalizedDb(peak * norm, m_params.floorDb, m_params.ceilingDb);

        switch (m_params.curve) {
        case ScalingCurve::Linear:
            break;
        case ScalingCurve::Logarithmic:
            v = float(std::log10(1.0 + 9.0 * double(v)));
            break;
        case ScalingCurve::Power:
            v = float(std::pow(double(v), std::max(0.01, m_params.gamma)));
            break;
        }

        // Rise fast, fall slow -- the asymmetry is what reads as "punchy".
        const float k = (v > m_smoothed[b]) ? attack : decay;
        m_smoothed[b] += (v - m_smoothed[b]) * k;
        out.bands[b] = std::clamp(m_smoothed[b], 0.0f, 1.0f);
    }

    // Aggregate bands into three broad energies for reactive layers.
    const double logMin = std::log(m_params.minHz);
    const double logMax = std::log(m_params.maxHz);
    float bassSum = 0.0f, midSum = 0.0f, trebleSum = 0.0f;
    int bassN = 0, midN = 0, trebleN = 0;

    for (int b = 0; b < count; ++b) {
        const double centreHz =
            std::exp(logMin + (logMax - logMin) * (double(b) + 0.5) / double(count));
        if (centreHz < 250.0) {
            bassSum += out.bands[b];
            ++bassN;
        } else if (centreHz < 2000.0) {
            midSum += out.bands[b];
            ++midN;
        } else {
            trebleSum += out.bands[b];
            ++trebleN;
        }
    }

    out.bass = bassN ? bassSum / float(bassN) : 0.0f;
    out.mid = midN ? midSum / float(midN) : 0.0f;
    out.treble = trebleN ? trebleSum / float(trebleN) : 0.0f;

    return out;
}

} // namespace mv
