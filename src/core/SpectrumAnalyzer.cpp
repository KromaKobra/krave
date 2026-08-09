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
    p.windowSamples = std::clamp(p.windowSamples, 64, 1 << 17);
    p.padFactor = std::clamp(p.padFactor, 1, 8);
    p.averageCount = std::clamp(p.averageCount, 1, 32);
    p.bandCount = std::clamp(p.bandCount, 1, 512);
    // minHz may be NEGATIVE. Bands below the first usable bin all collapse onto
    // it and render as a flat margin at the outer edge of the display, which is
    // how the reference visualizer places its lowest audible band inset from the
    // edge rather than at it.
    p.minHz = std::clamp(p.minHz, -20000.0, 20000.0);
    p.maxHz = std::max(p.minHz + 1.0, p.maxHz);
    p.spatialSmoothing = std::clamp(p.spatialSmoothing, 0.0, 1.0);
    p.floorLevel = std::clamp(p.floorLevel, 0.0, 1.0);
    p.bandNormalization = std::clamp(p.bandNormalization, 0.0, 1.0);

    // Callers hand the parameters over on EVERY frame, so this must distinguish
    // "the buffers need rebuilding" from "a value changed". Rebuilding
    // unconditionally would clear the smoothing envelope and the auto-gain
    // reference every frame, which silently turns attack/decay into a plain
    // gain and prevents auto-gain from ever converging.
    const bool structural = p.windowSamples != m_params.windowSamples
                         || p.padFactor != m_params.padFactor
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
    m_windowLen = m_params.windowSamples;
    m_fftSize = nextPowerOfTwo(m_windowLen * m_params.padFactor);

    m_fft = std::make_unique<Fft>(m_fftSize);

    // The Hann taper covers only the real window; the rest of the buffer stays
    // zero, so resolution follows windowSamples while the padding just adds
    // interpolated bins.
    m_window.resize(m_windowLen);
    for (int i = 0; i < m_windowLen; ++i)
        m_window[i] = float(0.5 * (1.0 - std::cos(2.0 * kPi * double(i)
                                                  / double(m_windowLen - 1))));

    m_frame.assign(m_fftSize, 0.0f);
    m_scratchRe.resize(m_fftSize);
    m_scratchIm.resize(m_fftSize);
    m_magnitudes.resize(m_fftSize / 2);
    m_power.resize(m_fftSize / 2);

    m_smoothed.assign(m_params.bandCount, 0.0f);
    m_raw.assign(m_params.bandCount, 0.0f);
    m_bandRefDb.assign(m_params.bandCount, float(m_params.floorDb));
    m_bandLo.resize(m_params.bandCount);
    m_bandHi.resize(m_params.bandCount);
    m_bandGainDb.resize(m_params.bandCount);
    m_centreHz.resize(m_params.bandCount);

    m_dirty = false;
    m_builtForRate = 0;
}

void SpectrumAnalyzer::layoutBands(int sampleRate)
{
    // Band edges depend on the sample rate. Spacing is logarithmic: octaves get
    // equal screen width, which is how people actually hear pitch.
    // Bin spacing comes from the PADDED size, so padding buys finer band
    // placement even though it does not change resolution.
    const int bins = m_fftSize / 2;
    const double binsPerHz = double(m_fftSize) / double(sampleRate);
    const int count = m_params.bandCount;

    // Logarithmic placement is undefined at or below zero, and minHz is allowed
    // to be negative. Clamp the endpoints used for the log term only; when the
    // blend is fully linear the log term is never evaluated.
    const double logLo = std::max(1e-3, m_params.minHz);
    const double logHi = std::max(logLo * 1.01, m_params.maxHz);
    const double logMin = std::log(logLo);
    const double logMax = std::log(logHi);

    const double k = std::clamp(m_params.hzLinearFactor, 0.0, 1.0);

    // Band edge for fraction t across the display, blended between logarithmic
    // and linear placement.
    const auto edgeHz = [&](double t) {
        const double linHz = m_params.minHz + (m_params.maxHz - m_params.minHz) * t;
        if (k >= 1.0)
            return linHz;
        const double logHz = std::exp(logMin + (logMax - logMin) * t);
        return logHz * (1.0 - k) + linHz * k;
    };

    for (int b = 0; b < count; ++b) {
        const double f0 = edgeHz(double(b) / double(count));
        const double f1 = edgeHz(double(b + 1) / double(count));

        // Kept fractional. Bin 0 is DC and is excluded.
        const double lo = std::clamp(f0 * binsPerHz, 1.0, double(bins - 1));
        const double hi = std::clamp(f1 * binsPerHz, lo, double(bins - 1));

        m_bandLo[b] = float(lo);
        m_bandHi[b] = float(hi);
        // Geometric centre is only meaningful for positive, log-spaced edges.
        m_centreHz[b] = float((k >= 0.5 || f0 <= 0.0) ? std::max(0.0, 0.5 * (f0 + f1))
                                                      : std::sqrt(f0 * f1));

        // Tilt is applied per band rather than per bin: the correction only
        // needs to follow the band layout, and this keeps it out of the inner
        // loop entirely.
        const double tiltRefHz = std::max(1e-3, m_params.minHz);
        const double octaves = std::log2(std::max(1e-6, double(m_centreHz[b]) / tiltRefHz));
        double gainDb = m_params.tiltDbPerOctave * octaves;

        // Low-frequency ramp, folded into the same per-band gain.
        if (m_params.lowFreqMagnitude < 1.0) {
            const double t = (count > 1) ? double(b) / double(count - 1) : 1.0;
            const double ramp = m_params.lowFreqMagnitude
                              + (1.0 - m_params.lowFreqMagnitude) * t;
            gainDb += 20.0 * std::log10(std::max(1e-4, ramp));
        }

        m_bandGainDb[b] = float(gainDb);
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

    // Welch averaging: several half-overlapped windows centred on the requested
    // timestamp, averaged in the power domain. readMono zero-pads the edges.
    const qint64 centre = buffer.frameForTime(timeSeconds);
    const int passes = std::max(1, m_params.averageCount);
    const int hop = m_windowLen / 2;
    const int bins = m_fftSize / 2;

    std::fill(m_power.begin(), m_power.end(), 0.0f);

    for (int p = 0; p < passes; ++p) {
        const qint64 offset = qint64(p - (passes - 1) / 2) * hop;
        // Only the leading windowLen samples are filled; the padding beyond
        // stays zero from rebuild() and must not be disturbed.
        buffer.readMono(centre - m_windowLen / 2 + offset, m_frame.data(), m_windowLen);
        for (int i = 0; i < m_windowLen; ++i)
            m_frame[i] *= m_window[i];

        m_fft->magnitudes(m_frame.constData(), m_magnitudes.data(),
                          m_scratchRe.data(), m_scratchIm.data());

        for (int i = 0; i < bins; ++i)
            m_power[i] += m_magnitudes[i] * m_magnitudes[i];
    }

    const float invPasses = 1.0f / float(passes);
    for (int i = 0; i < bins; ++i)
        m_magnitudes[i] = std::sqrt(m_power[i] * invPasses);

    // Normalize for window length and the Hann window's coherent gain, so the
    // dB scale means the same thing regardless of fftSize.
    // Coherent gain of the Hann taper over the TRUE window, so the amplitude
    // scale does not shift when the padding factor changes.
    const float norm = 2.0f / (float(m_windowLen) * 0.5f);
    const double span = m_params.ceilingDb - m_params.floorDb;
    const double invSpan = span > 0.0 ? 1.0 / span : 0.0;

    const int binCount = m_magnitudes.size();
    for (int b = 0; b < count; ++b) {
        const double lo = m_bandLo[b];
        const double hi = m_bandHi[b];

        float amplitude;
        if (hi - lo < 1.0) {
            // Narrower than one bin: read the spectrum at the band's centre,
            // interpolating between neighbouring bins. Snapping to whole bins
            // here is what produces a fixed comb across the display.
            const double c = 0.5 * (lo + hi);
            const int i0 = std::clamp(int(std::floor(c)), 0, binCount - 1);
            const int i1 = std::min(i0 + 1, binCount - 1);
            const double f = c - double(i0);
            amplitude = float(m_magnitudes[i0] * (1.0 - f) + m_magnitudes[i1] * f);
        } else if (m_params.measure == BandMeasure::Peak) {
            float peak = 0.0f;
            for (int i = int(lo); i <= int(hi) && i < binCount; ++i)
                peak = std::max(peak, m_magnitudes[i]);
            amplitude = peak;
        } else {
            // Energy average, weighting the partially covered bins at each end.
            // A single loud bin no longer defines the whole band, which stops
            // neighbouring bars from flickering independently.
            double sum = 0.0;
            double weight = 0.0;
            for (int i = int(std::floor(lo)); i <= int(std::floor(hi)) && i < binCount; ++i) {
                const double w = std::min(hi, double(i + 1)) - std::max(lo, double(i));
                if (w <= 0.0)
                    continue;
                sum += w * double(m_magnitudes[i]) * double(m_magnitudes[i]);
                weight += w;
            }
            amplitude = weight > 0.0 ? float(std::sqrt(sum / weight)) : 0.0f;
        }

        if (m_params.amplitude == AmplitudeMode::Linear) {
            // Straight magnitude. No floor/ceiling window and no gate: quiet
            // content simply stays near zero, which is what leaves a flat floor
            // with sharp peaks standing on it.
            double v = double(amplitude) * norm * m_params.linearGain;
            v *= std::pow(10.0, m_bandGainDb[b] / 20.0); // tilt, as a gain

            // Black level: drop the residual noise that otherwise holds every
            // band above the baseline, then rescale so the remaining range
            // still reaches 1.0.
            if (m_params.linearFloor > 0.0) {
                const double f = std::min(m_params.linearFloor, 0.99);
                v = (v - f) / (1.0 - f);
            }
            v = std::clamp(v, 0.0, 1.0);

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
            continue;
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

    const float symmetric = blendFactor(m_params.smoothTau, dt);
    for (int b = 0; b < count; ++b) {
        const float target = m_raw[b];
        // Asymmetric: rise fast, fall slow, which reads as "punchy" but makes
        // every bar fall at the same rate whatever the music does.
        // Symmetric: one constant both ways, which tracks the signal's own shape.
        const float k = (m_params.smoothing == SmoothingMode::Symmetric)
                            ? symmetric
                            : ((target > m_smoothed[b]) ? attack : decay);
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
