#pragma once

#include <AudioBuffer.h>
#include <SpectrumAnalyzer.h>

#include <QElapsedTimer>
#include <QObject>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <functional>
#include <unordered_map>

namespace mv {

class LayerModel;
class BarLayer;

/// One layer's analysis request for a single frame.
struct AnalysisRequest
{
    int layerId = 0;
    SpectrumParams params;
};

/// One layer's analysis result for a single frame.
struct AnalysisResult
{
    int layerId = 0;
    SpectrumFrame frame;
};

/// Runs spectrum analysis off the GUI thread.
///
/// Holds one SpectrumAnalyzer per layer id, so each bar layer keeps its own
/// smoothing envelope and its own band layout.
class SpectrumWorker : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Analyze every request against the buffer at `timeSeconds`. Callable
    /// directly (export) or through a queued connection (preview).
    QVector<AnalysisResult> analyze(const AudioBuffer &buffer, double timeSeconds, double dt,
                                    const QVector<AnalysisRequest> &requests);

public Q_SLOTS:
    void setBuffer(const mv::AudioBuffer &buffer);
    void requestAnalysis(double timeSeconds, double dt,
                         const QVector<mv::AnalysisRequest> &requests);
    void resetState();

Q_SIGNALS:
    void analysed(const QVector<mv::AnalysisResult> &results);

private:
    AudioBuffer m_buffer;
    // std::unordered_map rather than QHash: SpectrumAnalyzer owns a unique_ptr,
    // so it is movable but not copyable, and QHash requires copyable values.
    std::unordered_map<int, SpectrumAnalyzer> m_analyzers;
};

/// GUI-side controller: ticks at the preview frame rate, gathers each bar layer's
/// parameters, hands them to the worker, and writes results back into the layers.
///
/// Because the layers themselves hold the band data, the preview canvas and the
/// offscreen export canvas can bind to the very same Layer objects.
class SpectrumEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit SpectrumEngine(QObject *parent = nullptr);
    ~SpectrumEngine() override;

    void setLayerModel(LayerModel *model);
    void setBuffer(const AudioBuffer &buffer);

    /// Start/stop the live preview tick.
    void start();
    void stop();
    bool isRunning() const { return m_timer.isActive(); }

    /// Called each tick with the current playback position.
    void setTimeSource(std::function<double()> source) { m_timeSource = std::move(source); }

    /// Clear all smoothing envelopes, e.g. after a seek.
    void reset();

    /// Synchronous analysis for export, using a private analyzer set so it never
    /// disturbs the preview's smoothing state.
    void renderFrameSync(double timeSeconds, double dt);
    void resetExportState();

Q_SIGNALS:
    void runningChanged();
    /// Emitted after each frame's results have been applied to the layers.
    void frameApplied();

private:
    QVector<AnalysisRequest> collectRequests() const;
    void applyResults(const QVector<AnalysisResult> &results);
    void tick();

    LayerModel *m_model = nullptr;
    AudioBuffer m_buffer;

    QThread m_thread;
    SpectrumWorker *m_worker;          ///< lives on m_thread (preview)
    SpectrumWorker m_exportWorker;     ///< used synchronously on the caller's thread

    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastTickNs = 0;
    bool m_inFlight = false;

    std::function<double()> m_timeSource;
};

} // namespace mv

Q_DECLARE_METATYPE(mv::AnalysisRequest)
Q_DECLARE_METATYPE(mv::AnalysisResult)
Q_DECLARE_METATYPE(QVector<mv::AnalysisRequest>)
Q_DECLARE_METATYPE(QVector<mv::AnalysisResult>)
