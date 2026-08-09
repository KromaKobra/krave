#include "ExportController.h"

#include "AudioEngine.h"
#include "LayerModel.h"
#include "Project.h"
#include "SpectrumEngine.h"

#include <FfmpegLocator.h>

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>

#include <rhi/qrhi.h>

#include <cmath>

namespace mv {
namespace {

/// Pause rendering once this much data is queued for ffmpeg (roughly four 1080p
/// frames). Large enough to keep the encoder fed, small enough to bound memory.
constexpr qint64 kWriteHighWaterMark = 32 * 1024 * 1024;

constexpr auto kCanvasUrl = "qrc:/qt/qml/MusicVisualizer/VisualizerCanvas.qml";

} // namespace

ExportController::ExportController(QObject *parent)
    : QObject(parent)
{
    m_frameTimer.setInterval(0);
    m_frameTimer.setSingleShot(false);
    connect(&m_frameTimer, &QTimer::timeout, this, &ExportController::renderNextFrame);
}

ExportController::~ExportController()
{
    if (m_exporting)
        cancel();
}

void ExportController::setContext(QQmlEngine *engine, LayerModel *layers, AudioEngine *audio,
                                  SpectrumEngine *spectrum, Project *project)
{
    m_qmlEngine = engine;
    m_layers = layers;
    m_audio = audio;
    m_spectrum = spectrum;
    m_project = project;

    if (m_project) {
        m_width = m_project->canvasWidth();
        m_height = m_project->canvasHeight();
        Q_EMIT settingsChanged();
    }
}

double ExportController::progress() const
{
    if (m_totalFrames <= 0)
        return 0.0;
    return double(m_frameIndex) / double(m_totalFrames);
}

QString ExportController::suggestedExtension() const
{
    return m_videoCodec == QLatin1String("prores") ? QStringLiteral("mov")
                                                   : QStringLiteral("mp4");
}

void ExportController::setStatus(const QString &text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    Q_EMIT statusTextChanged();
}

void ExportController::fail(const QString &message)
{
    m_lastError = message;
    Q_EMIT lastErrorChanged();
    setStatus(message);
    finish(false);
}

// ------------------------------------------------------------------ scene setup

bool ExportController::buildScene()
{
    const QSize size(m_width, m_height);

    m_renderControl = std::make_unique<QQuickRenderControl>();
    m_window = std::make_unique<QQuickWindow>(m_renderControl.get());
    m_window->setGeometry(QRect(QPoint(0, 0), size));
    m_window->setColor(m_project ? m_project->canvasColor() : QColor(Qt::black));

    QQmlComponent component(m_qmlEngine, QUrl(QString::fromLatin1(kCanvasUrl)));
    if (component.isError()) {
        QStringList errors;
        for (const QQmlError &e : component.errors())
            errors << e.toString();
        fail(tr("Could not load the visualizer canvas:\n%1").arg(errors.join(QLatin1Char('\n'))));
        return false;
    }

    QObject *created = component.create(m_qmlEngine->rootContext());
    m_rootItem = qobject_cast<QQuickItem *>(created);
    if (!m_rootItem) {
        delete created;
        fail(tr("The visualizer canvas did not produce a renderable item."));
        return false;
    }

    // Bind to the very same layer objects the preview uses.
    m_rootItem->setProperty("layers", QVariant::fromValue(m_layers));
    if (m_project)
        m_rootItem->setProperty("canvasColor", m_project->canvasColor());

    m_rootItem->setParentItem(m_window->contentItem());
    m_rootItem->setWidth(m_width);
    m_rootItem->setHeight(m_height);

    if (!m_renderControl->initialize()) {
        fail(tr("Could not initialize offscreen rendering."));
        return false;
    }

    m_rhi = m_renderControl->rhi();
    if (!m_rhi) {
        fail(tr("No RHI backend is available for offscreen rendering."));
        return false;
    }

    // QQuickWindow::grabWindow() returns a null image for a render-control-backed
    // window, so the export renders into an explicit texture and reads that back.
    m_texture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                      QRhiTexture::RenderTarget
                                          | QRhiTexture::UsedAsTransferSource));
    if (!m_texture->create()) {
        fail(tr("Could not allocate a %1x%2 render texture.").arg(m_width).arg(m_height));
        return false;
    }

    m_depthStencil.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    if (!m_depthStencil->create()) {
        fail(tr("Could not allocate the depth-stencil buffer."));
        return false;
    }

    QRhiTextureRenderTargetDescription desc(QRhiColorAttachment(m_texture.get()));
    desc.setDepthStencilBuffer(m_depthStencil.get());
    m_renderTarget.reset(m_rhi->newTextureRenderTarget(desc));
    m_renderPass.reset(m_renderTarget->newCompatibleRenderPassDescriptor());
    m_renderTarget->setRenderPassDescriptor(m_renderPass.get());
    if (!m_renderTarget->create()) {
        fail(tr("Could not create the offscreen render target."));
        return false;
    }

    m_window->setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(m_renderTarget.get()));
    return true;
}

void ExportController::teardownScene()
{
    if (m_renderControl)
        m_renderControl->invalidate();

    m_renderTarget.reset();
    m_renderPass.reset();
    m_depthStencil.reset();
    m_texture.reset();
    m_rhi = nullptr;

    if (m_rootItem) {
        m_rootItem->setParentItem(nullptr);
        delete m_rootItem;
        m_rootItem = nullptr;
    }

    m_window.reset();
    m_renderControl.reset();
}

// ---------------------------------------------------------------------- ffmpeg

QStringList ExportController::buildFfmpegArgs() const
{
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-hide_banner")
         << QStringLiteral("-loglevel") << QStringLiteral("error");

    // Video comes in over stdin as bare RGBA, so its geometry must be declared.
    args << QStringLiteral("-f") << QStringLiteral("rawvideo")
         << QStringLiteral("-pixel_format") << QStringLiteral("rgba")
         << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(m_width).arg(m_height)
         << QStringLiteral("-framerate") << QString::number(m_fps)
         << QStringLiteral("-i") << QStringLiteral("-");

    const bool withAudio = m_includeAudio && m_audio && m_audio->hasTrack();
    if (withAudio)
        args << QStringLiteral("-i") << m_audio->trackPath();

    if (m_videoCodec == QLatin1String("prores")) {
        // ProRes 422 HQ.
        args << QStringLiteral("-c:v") << QStringLiteral("prores_ks")
             << QStringLiteral("-profile:v") << QStringLiteral("3")
             << QStringLiteral("-pix_fmt") << QStringLiteral("yuv422p10le");
    } else {
        args << QStringLiteral("-c:v") << QStringLiteral("libx264")
             << QStringLiteral("-preset") << QStringLiteral("medium")
             << QStringLiteral("-crf") << QString::number(m_quality)
             // yuv420p rather than the default yuv444p: some players and most
             // hardware decoders will not touch 4:4:4 H.264.
             << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    }

    if (withAudio) {
        if (m_videoCodec == QLatin1String("prores"))
            args << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le");
        else
            args << QStringLiteral("-c:a") << QStringLiteral("aac")
                 << QStringLiteral("-b:a") << QStringLiteral("320k");
        // Frame count and audio length can differ by a fraction of a frame.
        args << QStringLiteral("-shortest");
    } else {
        args << QStringLiteral("-an");
    }

    if (m_videoCodec != QLatin1String("prores"))
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");

    args << m_outputPath;
    return args;
}

bool ExportController::startProcess()
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::bytesWritten, this, [this](qint64) {
        // The pipe has drained enough; pick the render loop back up.
        if (m_paused && m_process->bytesToWrite() < kWriteHighWaterMark / 2) {
            m_paused = false;
            m_frameTimer.start();
        }
    });

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_exporting)
            return;
        if (error == QProcess::FailedToStart)
            fail(tr("Could not start ffmpeg at %1.").arg(ffmpeg::ffmpegPath()));
    });

    m_process->start(ffmpeg::ffmpegPath(), buildFfmpegArgs());
    if (!m_process->waitForStarted(10000)) {
        fail(tr("ffmpeg did not start: %1").arg(m_process->errorString()));
        return false;
    }
    return true;
}

// ------------------------------------------------------------------- lifecycle

bool ExportController::start(const QUrl &outputUrl)
{
    if (m_exporting)
        return false;

    if (!m_qmlEngine || !m_layers || !m_audio || !m_spectrum) {
        fail(tr("The export controller has not been wired up."));
        return false;
    }
    if (!m_audio->hasTrack()) {
        fail(tr("Import an audio track before exporting."));
        return false;
    }
    if (!ffmpeg::isAvailable()) {
        fail(tr("ffmpeg was not found."));
        return false;
    }

    m_outputPath = outputUrl.isLocalFile() ? outputUrl.toLocalFile() : outputUrl.toString();
    if (m_outputPath.isEmpty()) {
        fail(tr("No output file was chosen."));
        return false;
    }

    m_width = qMax(16, m_width - (m_width % 2));    // H.264 needs even dimensions
    m_height = qMax(16, m_height - (m_height % 2));
    m_fps = qBound(1, m_fps, 240);

    const double duration = m_audio->duration();
    m_totalFrames = int(std::ceil(duration * m_fps));
    if (m_totalFrames <= 0) {
        fail(tr("The track has no length to export."));
        return false;
    }

    m_lastError.clear();
    Q_EMIT lastErrorChanged();

    // Stop the live preview tick and playback: both would fight the export for
    // the layer band values, and there is no reason to burn cycles on them.
    m_spectrumWasRunning = m_spectrum->isRunning();
    m_spectrum->stop();
    m_audio->pause();
    m_spectrum->resetExportState();

    m_frameIndex = 0;
    m_etaSeconds = -1.0;
    m_paused = false;
    m_exporting = true;
    Q_EMIT exportingChanged();
    Q_EMIT progressChanged();
    setStatus(tr("Preparing…"));

    if (!buildScene())
        return false;
    if (!startProcess())
        return false;

    setStatus(tr("Rendering…"));
    m_clock.start();
    m_frameTimer.start();
    return true;
}

void ExportController::renderNextFrame()
{
    if (!m_exporting || !m_process)
        return;

    if (m_frameIndex >= m_totalFrames) {
        finish(true);
        return;
    }

    // Back off while ffmpeg catches up rather than queueing unbounded frames.
    if (m_process->bytesToWrite() >= kWriteHighWaterMark) {
        m_paused = true;
        m_frameTimer.stop();
        return;
    }

    if (m_process->state() != QProcess::Running) {
        fail(tr("ffmpeg exited unexpectedly: %1")
                 .arg(QString::fromUtf8(m_process->readAllStandardError()).trimmed()));
        return;
    }

    // Analysis is a pure function of (buffer, time), so asking for frame N's
    // timestamp reproduces exactly what the preview showed at that moment. The
    // dt passed here is the OUTPUT frame interval, which is what makes the
    // time-constant smoothing match across frame rates.
    const double t = double(m_frameIndex) / double(m_fps);
    m_spectrum->renderFrameSync(t, 1.0 / double(m_fps));

    m_renderControl->polishItems();
    m_renderControl->beginFrame();
    m_renderControl->sync();
    m_renderControl->render();

    QRhiReadbackResult readback;
    QRhiResourceUpdateBatch *batch = m_rhi->nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription(m_texture.get()), &readback);
    m_renderControl->commandBuffer()->resourceUpdate(batch);

    // endFrame() ends the offscreen frame and waits for GPU completion, so the
    // readback data is populated by the time it returns.
    m_renderControl->endFrame();

    if (readback.data.isEmpty()) {
        fail(tr("Frame %1 could not be read back from the GPU.").arg(m_frameIndex));
        return;
    }

    if (m_rhi->isYUpInFramebuffer()) {
        // Flip row order in place; D3D11 does not need this, OpenGL would.
        const int stride = m_width * 4;
        QByteArray flipped(readback.data.size(), Qt::Uninitialized);
        for (int y = 0; y < m_height; ++y) {
            std::memcpy(flipped.data() + y * stride,
                        readback.data.constData() + (m_height - 1 - y) * stride,
                        size_t(stride));
        }
        m_process->write(flipped);
    } else {
        m_process->write(readback.data);
    }

    ++m_frameIndex;

    const double elapsed = m_clock.elapsed() / 1000.0;
    if (m_frameIndex > 0 && elapsed > 0.25) {
        const double perFrame = elapsed / m_frameIndex;
        m_etaSeconds = perFrame * (m_totalFrames - m_frameIndex);
    }
    Q_EMIT progressChanged();
}

void ExportController::finish(bool success)
{
    if (!m_exporting)
        return;

    m_frameTimer.stop();
    m_paused = false;

    QString stderrText;
    if (m_process) {
        if (m_process->state() == QProcess::Running) {
            if (success) {
                // Close stdin so ffmpeg flushes and finalizes the container.
                m_process->closeWriteChannel();
                if (!m_process->waitForFinished(120000)) {
                    m_process->kill();
                    m_process->waitForFinished(2000);
                    success = false;
                    m_lastError = tr("ffmpeg did not finish in time.");
                    Q_EMIT lastErrorChanged();
                }
            } else {
                m_process->kill();
                m_process->waitForFinished(2000);
            }
        }

        stderrText = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (success && (m_process->exitStatus() != QProcess::NormalExit
                        || m_process->exitCode() != 0)) {
            success = false;
            m_lastError = stderrText.isEmpty()
                ? tr("ffmpeg exited with code %1.").arg(m_process->exitCode())
                : tr("ffmpeg failed: %1").arg(stderrText);
            Q_EMIT lastErrorChanged();
        }

        m_process->deleteLater();
        m_process = nullptr;
    }

    teardownScene();

    // Never leave a half-written file behind pretending to be a finished export.
    if (!success && !m_outputPath.isEmpty())
        QFile::remove(m_outputPath);

    m_exporting = false;
    Q_EMIT exportingChanged();

    if (success)
        setStatus(tr("Exported %1").arg(QFileInfo(m_outputPath).fileName()));
    else if (!m_lastError.isEmpty())
        setStatus(m_lastError);

    if (m_spectrumWasRunning)
        m_spectrum->start();

    Q_EMIT finished(success, m_outputPath);
}

void ExportController::cancel()
{
    if (!m_exporting)
        return;
    m_lastError = tr("Export cancelled.");
    Q_EMIT lastErrorChanged();
    finish(false);
}

} // namespace mv
