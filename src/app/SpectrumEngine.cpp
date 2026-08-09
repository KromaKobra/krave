#include "SpectrumEngine.h"
#include "LayerModel.h"

#include <QMetaObject>

namespace mv {

// ------------------------------------------------------------- SpectrumWorker

QVector<AnalysisResult> SpectrumWorker::analyze(const AudioBuffer &buffer, double timeSeconds,
                                                double dt,
                                                const QVector<AnalysisRequest> &requests)
{
    QVector<AnalysisResult> results;
    results.reserve(requests.size());

    for (const AnalysisRequest &req : requests) {
        SpectrumAnalyzer &analyzer = m_analyzers[req.layerId];
        analyzer.setParams(req.params);

        AnalysisResult r;
        r.layerId = req.layerId;
        r.frame = analyzer.analyze(buffer, timeSeconds, dt);
        results.append(r);
    }

    // Drop analyzers for layers that no longer exist, so a long editing session
    // does not accumulate state for deleted layers.
    if (m_analyzers.size() > size_t(requests.size())) {
        QSet<int> live;
        live.reserve(requests.size());
        for (const AnalysisRequest &req : requests)
            live.insert(req.layerId);
        for (auto it = m_analyzers.begin(); it != m_analyzers.end();)
            it = live.contains(it->first) ? std::next(it) : m_analyzers.erase(it);
    }

    return results;
}

void SpectrumWorker::setBuffer(const mv::AudioBuffer &buffer)
{
    m_buffer = buffer;
    resetState();
}

void SpectrumWorker::requestAnalysis(double timeSeconds, double dt,
                                     const QVector<mv::AnalysisRequest> &requests)
{
    Q_EMIT analysed(analyze(m_buffer, timeSeconds, dt, requests));
}

void SpectrumWorker::resetState()
{
    for (auto &entry : m_analyzers)
        entry.second.reset();
}

// ------------------------------------------------------------- SpectrumEngine

SpectrumEngine::SpectrumEngine(QObject *parent)
    : QObject(parent)
    , m_worker(new SpectrumWorker)
{
    qRegisterMetaType<mv::AudioBuffer>("mv::AudioBuffer");
    qRegisterMetaType<QVector<mv::AnalysisRequest>>("QVector<mv::AnalysisRequest>");
    qRegisterMetaType<QVector<mv::AnalysisResult>>("QVector<mv::AnalysisResult>");

    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &SpectrumWorker::analysed, this, [this](const QVector<AnalysisResult> &r) {
        m_inFlight = false;
        applyResults(r);
        Q_EMIT frameApplied();
    });
    m_thread.start();

    m_timer.setInterval(16); // ~60 Hz preview
    connect(&m_timer, &QTimer::timeout, this, &SpectrumEngine::tick);
    m_clock.start();
}

SpectrumEngine::~SpectrumEngine()
{
    m_thread.quit();
    m_thread.wait();
}

void SpectrumEngine::setLayerModel(LayerModel *model)
{
    m_model = model;
}

void SpectrumEngine::setBuffer(const AudioBuffer &buffer)
{
    m_buffer = buffer;
    QMetaObject::invokeMethod(m_worker, "setBuffer", Qt::QueuedConnection,
                              Q_ARG(mv::AudioBuffer, buffer));
    m_exportWorker.setBuffer(buffer);
}

void SpectrumEngine::start()
{
    if (m_timer.isActive())
        return;
    m_lastTickNs = m_clock.nsecsElapsed();
    m_timer.start();
    Q_EMIT runningChanged();
}

void SpectrumEngine::stop()
{
    if (!m_timer.isActive())
        return;
    m_timer.stop();
    Q_EMIT runningChanged();
}

void SpectrumEngine::reset()
{
    QMetaObject::invokeMethod(m_worker, "resetState", Qt::QueuedConnection);
}

void SpectrumEngine::resetExportState()
{
    m_exportWorker.resetState();
}

QVector<AnalysisRequest> SpectrumEngine::collectRequests() const
{
    QVector<AnalysisRequest> requests;
    if (!m_model)
        return requests;

    for (BarLayer *layer : m_model->spectrumLayers()) {
        AnalysisRequest req;
        req.layerId = layer->layerId();
        req.params = layer->analysisParams();
        requests.append(req);
    }
    return requests;
}

void SpectrumEngine::applyResults(const QVector<AnalysisResult> &results)
{
    if (!m_model)
        return;

    for (const AnalysisResult &r : results) {
        for (BarLayer *layer : m_model->spectrumLayers()) {
            if (layer->layerId() == r.layerId) {
                layer->applyFrame(r.frame);
                break;
            }
        }
    }
}

void SpectrumEngine::tick()
{
    const qint64 nowNs = m_clock.nsecsElapsed();
    const double dt = double(nowNs - m_lastTickNs) / 1e9;
    m_lastTickNs = nowNs;

    // Skip a tick rather than queueing up work the worker cannot keep up with.
    if (m_inFlight)
        return;

    const QVector<AnalysisRequest> requests = collectRequests();
    if (requests.isEmpty())
        return;

    const double time = m_timeSource ? m_timeSource() : 0.0;

    m_inFlight = true;
    QMetaObject::invokeMethod(m_worker, "requestAnalysis", Qt::QueuedConnection,
                              Q_ARG(double, time), Q_ARG(double, qBound(0.0, dt, 0.25)),
                              Q_ARG(QVector<mv::AnalysisRequest>, requests));
}

void SpectrumEngine::renderFrameSync(double timeSeconds, double dt)
{
    applyResults(m_exportWorker.analyze(m_buffer, timeSeconds, dt, collectRequests()));
}

} // namespace mv
