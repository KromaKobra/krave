#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QTimer>
#include <QUrl>
#include <memory>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QQuickItem;
class QQuickRenderControl;
class QQuickWindow;
class QProcess;
class QRhi;
class QRhiTexture;
class QRhiRenderBuffer;
class QRhiTextureRenderTarget;
class QRhiRenderPassDescriptor;
QT_END_NAMESPACE

namespace mv {

class AudioEngine;
class LayerModel;
class Project;
class SpectrumEngine;

/// Renders the visualizer offscreen and pipes raw frames to ffmpeg.
///
/// The offscreen window loads the SAME VisualizerCanvas.qml the preview uses and
/// binds it to the SAME Layer objects, so the export cannot drift from what the
/// user saw. Only the pacing differs: frames are produced as fast as the GPU and
/// the pipe allow rather than in real time.
///
/// Frames are produced on the GUI thread by a zero-interval timer -- one frame
/// per event-loop pass -- so the UI keeps repainting and progress keeps updating.
/// The loop suspends whenever ffmpeg's stdin backs up and resumes on bytesWritten;
/// without that, rendering at ~280 fps would outrun the encoder and the queued
/// frames (8 MB each at 1080p) would balloon memory.
class ExportController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int framesDone READ framesDone NOTIFY progressChanged)
    Q_PROPERTY(int totalFrames READ totalFrames NOTIFY progressChanged)
    Q_PROPERTY(double etaSeconds READ etaSeconds NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // --- settings, bound to the export dialog ---
    Q_PROPERTY(int width MEMBER m_width NOTIFY settingsChanged)
    Q_PROPERTY(int height MEMBER m_height NOTIFY settingsChanged)
    Q_PROPERTY(int fps MEMBER m_fps NOTIFY settingsChanged)
    Q_PROPERTY(bool includeAudio MEMBER m_includeAudio NOTIFY settingsChanged)
    /// "h264" or "prores". ProRes is only valid in a .mov container.
    Q_PROPERTY(QString videoCodec MEMBER m_videoCodec NOTIFY settingsChanged)
    /// x264 quality, lower is better. Ignored for ProRes.
    Q_PROPERTY(int quality MEMBER m_quality NOTIFY settingsChanged)

public:
    explicit ExportController(QObject *parent = nullptr);
    ~ExportController() override;

    void setContext(QQmlEngine *engine, LayerModel *layers, AudioEngine *audio,
                    SpectrumEngine *spectrum, Project *project);

    /// Begin an export to `outputUrl`. The container is chosen from its suffix.
    Q_INVOKABLE bool start(const QUrl &outputUrl);
    Q_INVOKABLE void cancel();

    /// Codec/extension pairs that make sense together, for the dialog to check.
    Q_INVOKABLE QString suggestedExtension() const;

    bool isExporting() const { return m_exporting; }
    double progress() const;
    int framesDone() const { return m_frameIndex; }
    int totalFrames() const { return m_totalFrames; }
    double etaSeconds() const { return m_etaSeconds; }
    QString statusText() const { return m_statusText; }
    QString lastError() const { return m_lastError; }

Q_SIGNALS:
    void exportingChanged();
    void progressChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void settingsChanged();
    void finished(bool success, const QString &outputPath);

private:
    bool buildScene();
    void teardownScene();
    bool startProcess();
    void renderNextFrame();
    void finish(bool success);
    void fail(const QString &message);
    void setStatus(const QString &text);
    QStringList buildFfmpegArgs() const;

    // Collaborators, not owned.
    QQmlEngine *m_qmlEngine = nullptr;
    LayerModel *m_layers = nullptr;
    AudioEngine *m_audio = nullptr;
    SpectrumEngine *m_spectrum = nullptr;
    Project *m_project = nullptr;

    // Offscreen scene.
    std::unique_ptr<QQuickRenderControl> m_renderControl;
    std::unique_ptr<QQuickWindow> m_window;
    QQuickItem *m_rootItem = nullptr;
    QRhi *m_rhi = nullptr;
    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiRenderBuffer> m_depthStencil;
    std::unique_ptr<QRhiTextureRenderTarget> m_renderTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_renderPass;

    QProcess *m_process = nullptr;
    QTimer m_frameTimer;
    QElapsedTimer m_clock;

    // Settings.
    int m_width = 1920;
    int m_height = 1080;
    int m_fps = 30;
    bool m_includeAudio = true;
    QString m_videoCodec = QStringLiteral("h264");
    int m_quality = 18;

    // Run state.
    bool m_exporting = false;
    bool m_paused = false;
    int m_frameIndex = 0;
    int m_totalFrames = 0;
    double m_etaSeconds = -1.0;
    QString m_outputPath;
    QString m_statusText;
    QString m_lastError;
    bool m_spectrumWasRunning = false;
};

} // namespace mv
