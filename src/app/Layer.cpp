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
