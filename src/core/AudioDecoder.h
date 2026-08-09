#pragma once

#include "AudioBuffer.h"

#include <QObject>
#include <QFutureWatcher>
#include <QString>

namespace mv {

/// Decodes any ffmpeg-readable audio file into an in-memory float PCM buffer.
///
/// Decoding shells out to ffmpeg rather than using a decode library: ffmpeg is
/// already a hard dependency for export, and it covers mp3/wav/flac/aac/m4a/ogg
/// with one code path and no extra third-party code.
class AudioDecoder : public QObject
{
    Q_OBJECT

public:
    /// Everything is resampled to this, so downstream code never deals with
    /// varying rates or channel counts.
    static constexpr int kTargetSampleRate = 48000;
    static constexpr int kTargetChannels = 2;

    explicit AudioDecoder(QObject *parent = nullptr);
    ~AudioDecoder() override;

    /// Start decoding. Returns false if a decode is already running or ffmpeg is
    /// unavailable, in which case `failed` has already been emitted.
    bool decode(const QString &filePath);

    bool isBusy() const { return m_busy; }

Q_SIGNALS:
    void busyChanged(bool busy);
    /// 0.0-1.0, or -1.0 when the total size could not be determined up front.
    void progress(double fraction);
    void decoded(const mv::AudioBuffer &buffer, const QString &filePath);
    void failed(const QString &message);

private:
    struct Result
    {
        AudioBuffer buffer;
        QString filePath;
        QString error;
    };

    void onFinished();
    void setBusy(bool busy);

    QFutureWatcher<Result> *m_watcher;
    bool m_busy = false;
};

} // namespace mv
