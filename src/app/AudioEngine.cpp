#include "AudioEngine.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QFileInfo>
#include <QMediaDevices>
#include <QUrl>

#include <algorithm>
#include <cstring>

namespace mv {

// ---------------------------------------------------------------- PcmCursorDevice

PcmCursorDevice::PcmCursorDevice(QObject *parent)
    : QIODevice(parent)
{
}

void PcmCursorDevice::setBuffer(const AudioBuffer &buffer)
{
    m_buffer = buffer;
    m_cursorFrame = 0;
    m_finishedEmitted = false;
}

void PcmCursorDevice::setCursorFrame(qint64 frame)
{
    m_cursorFrame = qBound<qint64>(0, frame, m_buffer.frameCount());
    m_finishedEmitted = false;
}

bool PcmCursorDevice::atEnd() const
{
    return m_cursorFrame >= m_buffer.frameCount();
}

qint64 PcmCursorDevice::bytesAvailable() const
{
    const qint64 framesLeft = std::max<qint64>(0, m_buffer.frameCount() - m_cursorFrame);
    return framesLeft * m_buffer.channels() * qint64(sizeof(float)) + QIODevice::bytesAvailable();
}

qint64 PcmCursorDevice::readData(char *data, qint64 maxSize)
{
    if (m_buffer.isEmpty() || maxSize <= 0)
        return 0;

    const int channels = m_buffer.channels();
    const qint64 frameBytes = qint64(channels) * qint64(sizeof(float));
    const qint64 framesLeft = m_buffer.frameCount() - m_cursorFrame;

    if (framesLeft <= 0) {
        if (!m_finishedEmitted) {
            m_finishedEmitted = true;
            Q_EMIT playbackFinished();
        }
        // Feed silence rather than returning 0: QAudioSink treats a short read as
        // an underrun and would stop the stream before the tail has been heard.
        const qint64 pad = std::min<qint64>(maxSize, 4096);
        std::memset(data, 0, size_t(pad));
        return pad;
    }

    const qint64 framesToRead = std::min(framesLeft, maxSize / frameBytes);
    if (framesToRead <= 0)
        return 0;

    const qint64 byteCount = framesToRead * frameBytes;
    const float *src = m_buffer.samples().constData() + m_cursorFrame * channels;
    std::memcpy(data, src, size_t(byteCount));

    m_cursorFrame += framesToRead;
    return byteCount;
}

qint64 PcmCursorDevice::writeData(const char *, qint64)
{
    return -1; // read-only
}

// -------------------------------------------------------------------- AudioEngine

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
    , m_decoder(new AudioDecoder(this))
    , m_device(new PcmCursorDevice(this))
{
    connect(m_decoder, &AudioDecoder::decoded, this, &AudioEngine::onDecoded);
    connect(m_decoder, &AudioDecoder::failed, this, &AudioEngine::onDecodeFailed);
    connect(m_decoder, &AudioDecoder::progress, this, [this](double p) {
        m_loadProgress = p;
        Q_EMIT loadProgressChanged();
    });

    // Queued: playbackFinished fires from inside readData(), and stop() tears down
    // the very sink and device that are mid-pull. Let the read return first.
    connect(m_device, &PcmCursorDevice::playbackFinished, this, [this] {
        // Bounce back to the start and idle, the way a transport normally behaves.
        stop();
    }, Qt::QueuedConnection);

    // Drives the playhead readout. The visualizer has its own tick.
    m_positionTimer.setInterval(16);
    connect(&m_positionTimer, &QTimer::timeout, this, &AudioEngine::positionChanged);
}

AudioEngine::~AudioEngine()
{
    if (m_sink)
        m_sink->stop();
}

QString AudioEngine::trackTitle() const
{
    if (m_trackPath.isEmpty())
        return {};
    return QFileInfo(m_trackPath).completeBaseName();
}

void AudioEngine::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void AudioEngine::setError(const QString &message)
{
    m_lastError = message;
    Q_EMIT lastErrorChanged();
    if (!message.isEmpty())
        Q_EMIT errorOccurred(message);
}

void AudioEngine::importFile(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty())
        return;

    stop();
    setError({});
    setLoading(true);
    m_loadProgress = 0.0;
    Q_EMIT loadProgressChanged();

    if (!m_decoder->decode(path))
        setLoading(false);
}

void AudioEngine::onDecodeFailed(const QString &message)
{
    setLoading(false);
    setError(message);
}

void AudioEngine::onDecoded(const mv::AudioBuffer &buffer, const QString &filePath)
{
    setLoading(false);

    m_buffer = buffer;
    m_trackPath = filePath;
    m_device->setBuffer(m_buffer);
    m_playheadFrame = 0;

    QVector<float> mins;
    QVector<float> maxs;
    m_buffer.computePeaks(kWaveformBuckets, mins, maxs);
    m_waveform.resize(mins.size() * 2);
    for (int i = 0; i < mins.size(); ++i) {
        m_waveform[i * 2] = qreal(mins[i]);
        m_waveform[i * 2 + 1] = qreal(maxs[i]);
    }

    rebuildSink();

    Q_EMIT trackChanged();
    Q_EMIT positionChanged();
}

void AudioEngine::rebuildSink()
{
    m_format = {};
    m_outputDevice = {};
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }

    QAudioFormat format;
    format.setSampleRate(m_buffer.sampleRate());
    format.setChannelCount(m_buffer.channels());
    format.setSampleFormat(QAudioFormat::Float);

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull()) {
        setError(tr("No audio output device is available."));
        return;
    }
    if (!output.isFormatSupported(format)) {
        setError(tr("The audio device does not support 32-bit float %1 Hz playback.")
                     .arg(m_buffer.sampleRate()));
        return;
    }

    // Validated here, once, so the per-run createSink() below stays silent.
    m_outputDevice = output;
    m_format = format;
    createSink();
}

bool AudioEngine::createSink()
{
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    if (!m_format.isValid() || m_outputDevice.isNull())
        return false;

    m_sink = std::make_unique<QAudioSink>(m_outputDevice, m_format);
    m_sink->setVolume(m_volume);
    m_sink->setBufferSize(m_format.bytesForDuration(kSinkBufferUSecs));

    connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State) {
        Q_EMIT playingChanged();
    });
    return true;
}

bool AudioEngine::isPlaying() const
{
    return m_sink && m_sink->state() == QAudio::ActiveState;
}

qint64 AudioEngine::audibleFrame() const
{
    if (!m_sink || m_sink->state() != QAudio::ActiveState)
        return qBound<qint64>(0, m_playheadFrame, m_buffer.frameCount());

    // Audio leaves the device in real time, so a wall clock anchored at the start
    // of the run tracks what is being heard. The sink's own byte counters do not:
    // bytesFree() reads zero immediately after start(), which makes the whole
    // buffer look like unplayed audio and drags the playhead backwards for the
    // first few hundred milliseconds of every run.
    const qint64 elapsed =
        m_playClock.nsecsElapsed() * qint64(m_buffer.sampleRate()) / 1'000'000'000LL;
    // Never claim to be past what has actually been read out of the buffer.
    const qint64 frame = std::min(m_startFrame + elapsed, m_device->cursorFrame());
    return qBound<qint64>(0, frame, m_buffer.frameCount());
}

void AudioEngine::resetDeviceTo(qint64 frame)
{
    // Destroy the sink rather than reset() it. reset() stops the stream and drops
    // what Qt still holds, but audio already handed down to the platform stream
    // survives and is heard as a burst from the old position on the next start.
    // Closing the device is the only thing that reliably discards it.
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }

    // Close and reopen so QIODevice's own read buffer goes with it; repositioning
    // the cursor alone would leave pre-seek bytes sitting a layer above us.
    m_device->close();
    m_device->setCursorFrame(frame);
    m_device->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
}

void AudioEngine::startSinkAt(qint64 frame)
{
    resetDeviceTo(frame);
    if (!createSink())
        return;

    m_startFrame = frame;
    m_playClock.start();
    m_sink->start(m_device);
    m_positionTimer.start();
}

double AudioEngine::position() const
{
    if (m_buffer.isEmpty())
        return 0.0;
    return qBound(0.0, m_buffer.timeForFrame(audibleFrame()), duration());
}

void AudioEngine::play()
{
    // The sink only exists while a run is in flight; the validated format is what
    // says whether playback is possible at all.
    if (m_buffer.isEmpty() || !m_format.isValid())
        return;
    if (m_sink && m_sink->state() == QAudio::ActiveState)
        return;

    if (m_playheadFrame >= m_buffer.frameCount())
        m_playheadFrame = 0;

    startSinkAt(m_playheadFrame);
    Q_EMIT playingChanged();
}

void AudioEngine::pause()
{
    if (!m_sink || m_sink->state() != QAudio::ActiveState)
        return;

    // Latch what was being heard before tearing the sink down; that value is what
    // the next run starts from, so pause/resume is seamless.
    m_playheadFrame = audibleFrame();
    resetDeviceTo(m_playheadFrame);
    m_positionTimer.stop();
    Q_EMIT playingChanged();
    Q_EMIT positionChanged();
}

void AudioEngine::togglePlay()
{
    isPlaying() ? pause() : play();
}

void AudioEngine::stop()
{
    m_playheadFrame = 0;
    resetDeviceTo(0);
    m_positionTimer.stop();
    Q_EMIT playingChanged();
    Q_EMIT positionChanged();
    Q_EMIT seeked();
}

void AudioEngine::seek(double seconds)
{
    if (m_buffer.isEmpty())
        return;

    m_playheadFrame = qBound<qint64>(0, m_buffer.frameForTime(seconds), m_buffer.frameCount());

    // Seeking while stopped or paused costs nothing but a cursor move; the sink is
    // only restarted when it was already running.
    if (m_sink && m_sink->state() == QAudio::ActiveState)
        startSinkAt(m_playheadFrame);
    else
        resetDeviceTo(m_playheadFrame);

    Q_EMIT playingChanged();
    Q_EMIT positionChanged();
    Q_EMIT seeked();
}

void AudioEngine::setVolume(qreal v)
{
    const qreal clamped = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(m_volume, clamped))
        return;
    m_volume = clamped;
    if (m_sink)
        m_sink->setVolume(m_volume);
    Q_EMIT volumeChanged();
}

} // namespace mv
