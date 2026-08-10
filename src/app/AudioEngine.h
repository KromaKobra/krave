#pragma once

#include <AudioBuffer.h>
#include <AudioDecoder.h>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <memory>

namespace mv {

/// Streams an in-memory AudioBuffer to QAudioSink and tracks a read cursor.
class PcmCursorDevice : public QIODevice
{
    Q_OBJECT

public:
    explicit PcmCursorDevice(QObject *parent = nullptr);

    void setBuffer(const AudioBuffer &buffer);

    /// Read position in frames.
    qint64 cursorFrame() const { return m_cursorFrame; }
    void setCursorFrame(qint64 frame);

    bool atEnd() const override;
    qint64 bytesAvailable() const override;
    /// A stream, not a file: keeps Qt from applying random-access buffering.
    bool isSequential() const override { return true; }

Q_SIGNALS:
    void playbackFinished();

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    AudioBuffer m_buffer;
    qint64 m_cursorFrame = 0;
    bool m_finishedEmitted = false;
};

/// Owns the decoded track and drives playback.
///
/// Decoding is delegated to mv::AudioDecoder (an ffmpeg subprocess on a worker
/// thread); this class handles the sink, the transport and the playhead.
class AudioEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)
    Q_PROPERTY(QString trackPath READ trackPath NOTIFY trackChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
    Q_PROPERTY(double duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(double loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    /// Downsampled waveform envelope for the timeline, interleaved as
    /// [min0, max0, min1, max1, ...] in the range -1..1.
    Q_PROPERTY(QList<qreal> waveform READ waveform NOTIFY trackChanged)

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    Q_INVOKABLE void importFile(const QUrl &url);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void stop();
    /// Jump to `seconds`. Safe to call while playing or stopped.
    Q_INVOKABLE void seek(double seconds);

    bool hasTrack() const { return !m_buffer.isEmpty(); }
    QString trackPath() const { return m_trackPath; }
    QString trackTitle() const;
    double duration() const { return m_buffer.duration(); }
    double position() const;
    bool isPlaying() const;
    bool isLoading() const { return m_loading; }
    double loadProgress() const { return m_loadProgress; }
    QString lastError() const { return m_lastError; }
    qreal volume() const { return m_volume; }
    void setVolume(qreal v);
    const QList<qreal> &waveform() const { return m_waveform; }

    /// The decoded PCM, for the analysis and export paths.
    const AudioBuffer &buffer() const { return m_buffer; }

Q_SIGNALS:
    void trackChanged();
    void positionChanged();
    /// The playhead jumped rather than advanced; smoothing state should be dropped.
    void seeked();
    void playingChanged();
    void loadingChanged();
    void loadProgressChanged();
    void lastErrorChanged();
    void volumeChanged();
    void errorOccurred(const QString &message);

private:
    void onDecoded(const mv::AudioBuffer &buffer, const QString &filePath);
    void onDecodeFailed(const QString &message);
    void rebuildSink();
    void setLoading(bool loading);
    void setError(const QString &message);

    /// Build a sink from the validated format. False if there is no usable format.
    bool createSink();
    /// The frame the listener is actually hearing right now.
    qint64 audibleFrame() const;
    /// Tear the sink down and reposition the device at `frame`.
    void resetDeviceTo(qint64 frame);
    /// resetDeviceTo() plus a fresh sink started at `frame`.
    void startSinkAt(qint64 frame);

    /// How much audio the sink is allowed to queue. The platform default is around
    /// half a second, which makes seeks feel laggy and leaves a long stale tail.
    static constexpr qint64 kSinkBufferUSecs = 100'000;

    static constexpr int kWaveformBuckets = 2000;

    AudioDecoder *m_decoder;
    AudioBuffer m_buffer;
    QString m_trackPath;
    QList<qreal> m_waveform;

    std::unique_ptr<QAudioSink> m_sink;
    PcmCursorDevice *m_device;
    QTimer m_positionTimer;

    /// Validated once per track, then reused to build a sink per playback run.
    QAudioFormat m_format;
    QAudioDevice m_outputDevice;

    /// Authoritative playhead whenever the sink is not running. While it is, the
    /// playhead is m_startFrame plus however long m_playClock says we have been
    /// playing -- audio leaves the device in real time, so a wall clock tracks it
    /// far more steadily than the sink's own byte counters do.
    qint64 m_playheadFrame = 0;
    qint64 m_startFrame = 0;
    QElapsedTimer m_playClock;

    bool m_loading = false;
    double m_loadProgress = 0.0;
    QString m_lastError;
    qreal m_volume = 1.0;
};

} // namespace mv
