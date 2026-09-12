#include "Node.h"

#include <QSet>
#include <algorithm>

namespace mv {

// ---------------------------------------------------------------------- Node

Node::Node(QObject *parent)
    : QObject(parent)
{
}

const NodeTypeSpec &Node::spec() const
{
    // Every concrete subclass has a table entry, and tst_graph pins that the two
    // lists agree. The fallback exists so a missing entry degrades to a
    // port-less node rather than dereferencing null.
    static const NodeTypeSpec unknown;
    const NodeTypeSpec *found = nodeTypeSpec(typeId());
    return found ? *found : unknown;
}

QVector<Node *> &Node::slots_() const
{
    const int n = spec().inputs.size();
    if (m_inputs.size() != n)
        m_inputs.resize(n);
    return m_inputs;
}

int Node::inputCount() const
{
    return int(slots_().size());
}

Node *Node::input(int port) const
{
    const QVector<Node *> &in = slots_();
    if (port < 0 || port >= in.size())
        return nullptr;
    return in.at(port);
}

int Node::portIndex(const QString &portName) const
{
    const QVector<PortSpec> &ports = spec().inputs;
    for (int i = 0; i < ports.size(); ++i) {
        if (ports.at(i).name == portName)
            return i;
    }
    return -1;
}

Node *Node::input(const QString &portName) const
{
    return input(portIndex(portName));
}

int Node::sourceId(const QString &portName) const
{
    const Node *source = input(portName);
    return source ? source->nodeId() : 0;
}

void Node::setNodeId(int id)
{
    if (m_nodeId == id)
        return;
    m_nodeId = id;
    Q_EMIT nodeIdChanged();
}

void Node::setInput(int port, Node *source)
{
    QVector<Node *> &in = slots_();
    if (port < 0 || port >= in.size())
        return;
    in[port] = source;
}

// ---------------------------------------------------------------- VisualNode

VisualNode::VisualNode(QObject *parent)
    : Node(parent)
{
}

// ----------------------------------------------------------- AudioSourceNode

AudioSourceNode::AudioSourceNode(QObject *parent)
    : Node(parent)
{
}

void AudioSourceNode::applyFrame(const SpectrumFrame &frame)
{
    m_frame = frame;

    const int n = frame.bands.size();
    if (m_bands.size() != n)
        m_bands.resize(n);
    for (int i = 0; i < n; ++i)
        m_bands[i] = qreal(frame.bands[i]);

    Q_EMIT bandsChanged();
}

const Node *AudioSourceNode::audioRoot() const
{
    // The chain cannot be cyclic if it was built through NodeGraph::link, but a
    // hand-edited file bypasses that entirely -- and an unguarded walk over
    // A->B->A never returns.
    QSet<const Node *> seen;
    const Node *n = this;
    while (n) {
        if (seen.contains(n)) {
            qWarning("mv: cycle in the audio chain at node %d", n->nodeId());
            return nullptr;
        }
        seen.insert(n);

        const Node *next = n->input(QStringLiteral("audio"));
        if (!next)
            break;
        n = next;
    }

    // Only an AudioNode actually holds PCM. A separator whose chain ends at
    // another separator with nothing plugged in has no samples to analyse.
    return qobject_cast<const AudioNode *>(n) ? n : nullptr;
}

// ----------------------------------------------------------------- AudioNode

AudioNode::AudioNode(QObject *parent)
    : AudioSourceNode(parent)
{
    m_name = tr("Audio");
    m_bands.assign(1, 0.0);
}

SpectrumParams AudioNode::analysisParams() const
{
    // One band over everything audible: this node is the track's overall level,
    // and anything wanting a real band layout wires a separator in after it.
    SpectrumParams p;
    p.bandCount = 1;
    p.minHz = 20.0;
    p.maxHz = 20000.0;
    p.amplitude = AmplitudeMode::Decibel;
    p.attackTau = 0.01;
    p.decayTau = 0.15;
    p.bandNormalization = 0.0;
    return p;
}

void AudioNode::applyFrame(const SpectrumFrame &frame)
{
    AudioSourceNode::applyFrame(frame);
    m_level = m_bands.isEmpty() ? 0.0 : m_bands.first();
}

// ------------------------------------------------------------- SeparatorNode

SeparatorNode::SeparatorNode(QObject *parent)
    : AudioSourceNode(parent)
{
    m_name = tr("Separator");
    m_bands.assign(m_bandCount, 0.0);
}

SpectrumParams SeparatorNode::analysisParams() const
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
    p.linearFloor = m_linearFloor;
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

void SeparatorNode::applyFrame(const SpectrumFrame &frame)
{
    AudioSourceNode::applyFrame(frame);
    m_bass = qreal(frame.bass);
    m_mid = qreal(frame.mid);
    m_treble = qreal(frame.treble);
}

// -------------------------------------------------------------- SpectrumNode

SpectrumNode::SpectrumNode(QObject *parent)
    : VisualNode(parent)
{
    m_name = tr("Spectrum");
    m_sizeH = 0.5;
    m_posY = 0.75;
}

void SpectrumNode::takeFrame(const SpectrumFrame &frame)
{
    const int n = frame.bands.size();
    m_inactiveEdgeBars = frame.activeFrom;

    if (m_mirrorSpectrum) {
        // The band order is repeated back on itself. An even total keeps the two
        // halves the same width so the result is exactly symmetric.
        //
        // Default: lowest band at both outer edges, highest meeting in the
        // centre. With `bassInner` the order flips -- the lowest band lands in
        // the two centre slots and the highest sits at the edges.
        const int total = n * 2;
        if (m_bands.size() != total)
            m_bands.resize(total);
        for (int i = 0; i < n; ++i) {
            const qreal v = qreal(frame.bands[i]);
            if (m_bassInner) {
                m_bands[n - 1 - i] = v; // left half, running outward from the centre
                m_bands[n + i] = v;     // right half
            } else {
                m_bands[i] = v;             // left half, running inward from the edge
                m_bands[total - 1 - i] = v; // right half
            }
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

void SpectrumNode::clearFrame()
{
    if (m_bands.isEmpty() && m_inactiveEdgeBars == 0 && m_bass == 0.0 && m_mid == 0.0
        && m_treble == 0.0) {
        return;
    }

    m_bands.clear();
    m_inactiveEdgeBars = 0;
    m_bass = 0.0;
    m_mid = 0.0;
    m_treble = 0.0;
    Q_EMIT bandsChanged();
}

// ------------------------------------------------------------ BackgroundNode

BackgroundNode::BackgroundNode(QObject *parent)
    : VisualNode(parent)
{
    m_name = tr("Background");
}

// ------------------------------------------------------------------ TextNode

TextNode::TextNode(QObject *parent)
    : VisualNode(parent)
{
    m_name = tr("Text");
    m_posY = 0.25;
}

// ---------------------------------------------------------------- OutputNode

OutputNode::OutputNode(QObject *parent)
    : Node(parent)
{
    m_name = tr("Output");
}

} // namespace mv
