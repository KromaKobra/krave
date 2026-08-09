#include "Layer.h"

#include <algorithm>

namespace mv {

Layer::Layer(QObject *parent)
    : QObject(parent)
{
}

BarLayer::BarLayer(QObject *parent)
    : Layer(parent)
{
    m_name = tr("Spectrum bars");
    m_sizeH = 0.5;
    m_posY = 0.75;
    m_bands.assign(m_barCount, 0.0);
}

void BarLayer::applyFrame(const SpectrumFrame &frame)
{
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

SpectrumParams BarLayer::analysisParams() const
{
    SpectrumParams p;
    p.fftSize = m_fftSize;
    p.bandCount = std::max(1, m_barCount);
    p.minHz = m_minHz;
    p.maxHz = m_maxHz;
    p.floorDb = m_floorDb;
    p.ceilingDb = m_ceilingDb;

    if (m_scalingCurve == QLatin1String("log"))
        p.curve = ScalingCurve::Logarithmic;
    else if (m_scalingCurve == QLatin1String("power"))
        p.curve = ScalingCurve::Power;
    else
        p.curve = ScalingCurve::Linear;

    p.gamma = m_gamma;
    p.attackTau = std::max(0.0, m_attackMs) / 1000.0;
    p.decayTau = std::max(0.0, m_decayMs) / 1000.0;
    return p;
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
