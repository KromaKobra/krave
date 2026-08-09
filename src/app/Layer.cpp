#include "Layer.h"

#include <algorithm>

namespace mv {

Layer::Layer(QObject *parent)
    : QObject(parent)
{
}

AudioProviderLayer::AudioProviderLayer(QObject *parent)
    : Layer(parent)
{
    m_name = tr("Audio provider");
    m_bands.assign(m_bandCount, 0.0);
}

SpectrumParams AudioProviderLayer::analysisParams() const
{
    SpectrumParams p;
    // Window length is authored in milliseconds because that is what sets
    // frequency resolution, and it is not rounded to a power of two -- the
    // analyzer zero-pads instead. 48 kHz is assumed here only to convert the
    // duration; the band layout uses the real rate.
    p.windowSamples = int(std::lround(m_windowMs * 0.001 * 48000.0));
    p.bandCount = std::max(1, m_bandCount);
    p.minHz = m_minHz;
    p.maxHz = m_maxHz;
    p.hzLinearFactor = m_hzLinearFactor;
    p.averageCount = m_averageCount;
    p.measure = (m_bandMeasure == QLatin1String("peak")) ? BandMeasure::Peak
                                                         : BandMeasure::Rms;
    p.amplitude = (m_amplitudeMode == QLatin1String("linear")) ? AmplitudeMode::Linear
                                                               : AmplitudeMode::Decibel;
    p.linearGain = m_linearGain;
    p.lowFreqMagnitude = m_lowFreqMagnitude;
    p.smoothing = (m_smoothingMode == QLatin1String("symmetric"))
                      ? SmoothingMode::Symmetric
                      : SmoothingMode::AttackDecay;
    p.smoothTau = std::max(0.0, m_smoothMs) / 1000.0;
    p.tiltDbPerOctave = m_tiltDbPerOctave;
    p.bandNormalization = m_bandNormalization;
    p.normalizationTau = m_normalizationTau;
    p.floorDb = m_floorDb;
    p.ceilingDb = m_ceilingDb;
    p.gateDb = m_gateDb;

    if (m_scalingCurve == QLatin1String("log"))
        p.curve = ScalingCurve::Logarithmic;
    else if (m_scalingCurve == QLatin1String("linear"))
        p.curve = ScalingCurve::Linear;
    else
        p.curve = ScalingCurve::Power;

    p.gamma = m_gamma;
    p.attackTau = std::max(0.0, m_attackMs) / 1000.0;
    p.decayTau = std::max(0.0, m_decayMs) / 1000.0;
    p.spatialSmoothing = m_spatialSmoothing;
    p.floorLevel = m_floorLevel;
    return p;
}

void AudioProviderLayer::applyFrame(const SpectrumFrame &frame)
{
    m_frame = frame;

    const int n = frame.bands.size();
    if (m_bands.size() != n)
        m_bands.resize(n);
    for (int i = 0; i < n; ++i)
        m_bands[i] = qreal(frame.bands[i]);

    m_bass = qreal(frame.bass);
    m_mid = qreal(frame.mid);
    m_treble = qreal(frame.treble);

    Q_EMIT bandsChanged();
}

BarLayer::BarLayer(QObject *parent)
    : Layer(parent)
{
    m_name = tr("Spectrum bars");
    m_sizeH = 0.5;
    m_posY = 0.75;
}

void BarLayer::takeFrame(const SpectrumFrame &frame)
{
    const int n = frame.bands.size();

    if (m_mirrorSpectrum) {
        // Lowest band at both outer edges, highest meeting in the centre. An
        // even total keeps the two halves the same width so the result is
        // exactly symmetric.
        const int total = n * 2;
        if (m_bands.size() != total)
            m_bands.resize(total);
        for (int i = 0; i < n; ++i) {
            const qreal v = qreal(frame.bands[i]);
            m_bands[i] = v;             // left half, running inward from the edge
            m_bands[total - 1 - i] = v; // right half
        }
    } else {
        if (m_bands.size() != n)
            m_bands.resize(n);
        for (int i = 0; i < n; ++i)
            m_bands[i] = qreal(frame.bands[i]);
    }

    m_bass = qreal(frame.bass);
    m_mid = qreal(frame.mid);
    m_treble = qreal(frame.treble);

    Q_EMIT bandsChanged();
}

ParticleLayer::ParticleLayer(QObject *parent)
    : Layer(parent)
{
    m_name = tr("Particles");
    m_posY = 0.8;
    m_sizeH = 0.1;
}

SpectrumParams ParticleLayer::analysisParams() const
{
    // A particle layer only needs the three aggregate energies, so it runs a
    // coarse band layout -- no point paying for 64 bands it will not read.
    SpectrumParams p;
    p.bandCount = 24;
    p.attackTau = 0.005;
    p.decayTau = 0.120;
    return p;
}

void ParticleLayer::applyFrame(const SpectrumFrame &frame)
{
    qreal value;
    if (m_band == QLatin1String("treble"))
        value = qreal(frame.treble);
    else if (m_band == QLatin1String("mid"))
        value = qreal(frame.mid);
    else
        value = qreal(frame.bass);

    if (qFuzzyCompare(m_level + 1.0, value + 1.0))
        return;

    m_level = value;
    Q_EMIT levelChanged();
}

BackgroundLayer::BackgroundLayer(QObject *parent)
    : Layer(parent)
{
    m_name = tr("Background");
}

TextLayer::TextLayer(QObject *parent)
    : Layer(parent)
{
    m_name = tr("Text");
    m_posY = 0.25;
}

} // namespace mv
