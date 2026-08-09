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

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer()
{
    rebuild();
}

void SpectrumAnalyzer::setParams(const SpectrumParams &params)
{
    SpectrumParams p = params;
    p.fftSize = std::clamp(nextPowerOfTwo(std::max(64, p.fftSize)), 64, 16384);
    p.bandCount = std::clamp(p.bandCount, 1, 512);
    p.minHz = std::max(1.0, p.minHz);
    p.maxHz = std::max(p.minHz * 1.01, p.maxHz);
    p.spatialSmoothing = std::clamp(p.spatialSmoothing, 0.0, 1.0);
    p.floorLevel = std::clamp(p.floorLevel, 0.0, 1.0);
    p.bandNormalization = std::clamp(p.bandNormalization, 0.0, 1.0);

    // Callers hand the parameters over on EVERY frame, so this must distinguish
    // "the buffers need rebuilding" from "a value changed". Rebuilding
    // unconditionally would clear the smoothing envelope and the auto-gain
    // reference every frame, which silently turns attack/decay into a plain
    // gain and prevents auto-gain from ever converging.
    const bool structural = p.fftSize != m_params.fftSize
                         || p.bandCount != m_params.bandCount;
    const bool relayout = structural
                       || p.minHz != m_params.minHz
                       || p.maxHz != m_params.maxHz
                       || p.tiltDbPerOctave != m_params.tiltDbPerOctave;

    m_params = p;

    if (structural)
        m_dirty = true;
    if (relayout)
        m_builtForRate = 0; // forces layoutBands on the next analyze()
}

void SpectrumAnalyzer::reset()
{
    m_smoothed.fill(0.0f);
    m_bandRefDb.fill(float(m_params.floorDb));
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
    m_raw.assign(m_params.bandCount, 0.0f);
    m_bandRefDb.assign(m_params.bandCount, float(m_params.floorDb));
    m_bandStart.resize(m_params.bandCount);
    m_bandEnd.resize(m_params.bandCount);
    m_bandGainDb.resize(m_params.bandCount);
    m_centreHz.resize(m_params.bandCount);

    m_dirty = false;
    m_builtForRate = 0;
}

void SpectrumAnalyzer::layoutBands(int sampleRate)
{
    // Band edges depend on the sample rate. Spacing is logarithmic: octaves get
    // equal screen width, which is how people actually hear pitch.
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
        m_centreHz[b] = float(std::sqrt(f0 * f1));

        // Tilt is applied per band rather than per bin: the correction only
        // needs to follow the band layout, and this keeps it out of the inner
        // loop entirely.
        const double octaves = std::log2(double(m_centreHz[b]) / m_params.minHz);
        m_bandGainDb[b] = float(m_params.tiltDbPerOctave * octaves);
    }

    m_builtForRate = sampleRate;
}

SpectrumFrame SpectrumAnalyzer::analyze(const AudioBuffer &buffer, double timeSeconds, double dt)
{
    const int sampleRate = buffer.sampleRate() > 0 ? buffer.sampleRate() : 48000;

    if (m_dirty)
        rebuild();
    if (sampleRate != m_builtForRate)
        layoutBands(sampleRate);

    const int n = m_params.fftSize;
    const int count = m_params.bandCount;
    const float attack = blendFactor(m_params.attackTau, dt);
    const float decay = blendFactor(m_params.decayTau, dt);
    const float floorLevel = float(m_params.floorLevel);

    SpectrumFrame out;
    out.bands.resize(count);

    if (buffer.isEmpty()) {
        // Still decay toward the resting level so bars fall away rather than freeze.
        for (int b = 0; b < count; ++b) {
            m_smoothed[b] += (0.0f - m_smoothed[b]) * decay;
            out.bands[b] = std::max(m_smoothed[b], floorLevel);
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
    const double span = m_params.ceilingDb - m_params.floorDb;
    const double invSpan = span > 0.0 ? 1.0 / span : 0.0;

    for (int b = 0; b < count; ++b) {
        const int from = m_bandStart[b];
        const int to = m_bandEnd[b];

        float amplitude;
        if (m_params.measure == BandMeasure::Peak) {
            float peak = 0.0f;
            for (int i = from; i < to; ++i)
                peak = std::max(peak, m_magnitudes[i]);
            amplitude = peak;
        } else {
            // Energy average. A single loud bin no longer defines the whole band,
            // which is what stops neighbouring bars from flickering independently.
            double sum = 0.0;
            for (int i = from; i < to; ++i)
                sum += double(m_magnitudes[i]) * double(m_magnitudes[i]);
            amplitude = float(std::sqrt(sum / double(to - from)));
        }

        double db = 20.0 * std::log10(double(amplitude * norm) + 1e-12);
        db += m_bandGainDb[b];

        if (m_params.bandNormalization > 0.0) {
            // Slow-moving average of this band's level, in dB.
            float &ref = m_bandRefDb[b];
            const float k = blendFactor(m_params.normalizationTau, dt);
            ref += (float(db) - ref) * k;

            // Re-centre the band on a common target. Deviations from the band's
            // own average survive untouched, so contrast is preserved while the
            // overall slope across bands is flattened.
            const double target = m_params.ceilingDb - 0.35 * span;
            const double normalized = db - double(ref) + target;
            db += (normalized - db) * m_params.bandNormalization;
        }

        // Soft gate: fade out across `gateDb` above the floor instead of
        // clipping, so quiet passages settle to nothing without popping.
        double gate = 1.0;
        if (m_params.gateDb > 0.0) {
            const double over = db - m_params.floorDb;
            gate = std::clamp(over / m_params.gateDb, 0.0, 1.0);
            gate *= gate;
        }

        double v = std::clamp((db - m_params.floorDb) * invSpan, 0.0, 1.0) * gate;

        switch (m_params.curve) {
        case ScalingCurve::Linear:
            break;
        case ScalingCurve::Logarithmic:
            v = std::log10(1.0 + 9.0 * v);
            break;
        case ScalingCurve::Power:
            v = std::pow(v, std::max(0.01, m_params.gamma));
            break;
        }

        m_raw[b] = float(v);
    }

    // Blur across neighbours. Without this each band is an independent estimate
    // and the row of bars reads as noise rather than as one moving contour.
    if (m_params.spatialSmoothing > 0.0 && count > 2) {
        const float s = float(m_params.spatialSmoothing);
        static thread_local QVector<float> scratch;
        scratch = m_raw;
        for (int b = 0; b < count; ++b) {
            const float l = scratch[std::max(0, b - 1)];
            const float r = scratch[std::min(count - 1, b + 1)];
            m_raw[b] = (1.0f - s) * scratch[b] + s * 0.5f * (l + r);
        }
    }

    for (int b = 0; b < count; ++b) {
        // Rise fast, fall slow -- the asymmetry is what reads as "punchy".
        const float target = m_raw[b];
        const float k = (target > m_smoothed[b]) ? attack : decay;
        m_smoothed[b] += (target - m_smoothed[b]) * k;
        out.bands[b] = std::max(std::clamp(m_smoothed[b], 0.0f, 1.0f), floorLevel);
    }

    // Aggregate bands into three broad energies for reactive layers.
    float bassSum = 0.0f, midSum = 0.0f, trebleSum = 0.0f;
    int bassN = 0, midN = 0, trebleN = 0;
    for (int b = 0; b < count; ++b) {
        if (m_centreHz[b] < 250.0f) {
            bassSum += out.bands[b];
            ++bassN;
        } else if (m_centreHz[b] < 2000.0f) {
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
