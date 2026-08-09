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

    connect(m_device, &PcmCursorDevice::playbackFinished, this, [this] {
        // Bounce back to the start and idle, the way a transport normally behaves.
        stop();
    });

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

    m_sink = std::make_unique<QAudioSink>(output, format);
    m_sink->setVolume(m_volume);

    connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State) {
        Q_EMIT playingChanged();
    });
}

bool AudioEngine::isPlaying() const
{
    return m_sink && m_sink->state() == QAudio::ActiveState;
}

double AudioEngine::position() const
{
    if (m_buffer.isEmpty())
        return 0.0;

    // The cursor is where the device has been READ to, which runs ahead of what
    // the speakers have actually produced by however much the sink has buffered.
    // Subtract that so the playhead lines up with what the user is hearing.
    qint64 frame = m_device->cursorFrame();
    if (m_sink && m_sink->state() == QAudio::ActiveState) {
        const qint64 pendingBytes = m_sink->bufferSize() - m_sink->bytesFree();
        const qint64 frameBytes = qint64(m_buffer.channels()) * qint64(sizeof(float));
        if (frameBytes > 0)
            frame -= pendingBytes / frameBytes;
    }

    return qBound(0.0, m_buffer.timeForFrame(std::max<qint64>(0, frame)), duration());
}

void AudioEngine::play()
{
    if (m_buffer.isEmpty() || !m_sink)
        return;
    if (m_sink->state() == QAudio::ActiveState)
        return;

    if (m_device->atEnd())
        m_device->setCursorFrame(0);

    if (!m_device->isOpen())
        m_device->open(QIODevice::ReadOnly);

    if (m_sink->state() == QAudio::SuspendedState)
        m_sink->resume();
    else
        m_sink->start(m_device);

    m_positionTimer.start();
    Q_EMIT playingChanged();
}

void AudioEngine::pause()
{
    if (!m_sink || m_sink->state() != QAudio::ActiveState)
        return;
    m_sink->suspend();
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
    if (m_sink)
        m_sink->stop();
    m_positionTimer.stop();
    m_device->setCursorFrame(0);
    Q_EMIT playingChanged();
    Q_EMIT positionChanged();
}

void AudioEngine::seek(double seconds)
{
    if (m_buffer.isEmpty())
        return;

    const bool wasPlaying = isPlaying();
    const qint64 frame = qBound<qint64>(0, m_buffer.frameForTime(seconds), m_buffer.frameCount());

    if (m_sink) {
        // reset() drops whatever is already queued in the sink; without it the
        // old audio keeps playing for the length of the buffer after a seek.
        m_sink->stop();
        m_sink->reset();
    }

    m_device->setCursorFrame(frame);

    if (wasPlaying) {
        if (!m_device->isOpen())
            m_device->open(QIODevice::ReadOnly);
        m_sink->start(m_device);
        m_positionTimer.start();
    }

    Q_EMIT playingChanged();
    Q_EMIT positionChanged();
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
