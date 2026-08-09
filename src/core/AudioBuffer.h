#pragma once

#include <QVector>
#include <QMetaType>
#include <QtGlobal>

namespace mv {

/// Fully decoded, interleaved 32-bit float PCM held in memory.
///
/// The whole track lives here for the lifetime of the session. That is what lets
/// spectrum analysis be a pure function of (buffer, time) rather than something
/// driven by a live audio tap -- preview and export can then ask for the exact
/// same window of samples and get the exact same answer.
class AudioBuffer
{
public:
    AudioBuffer() = default;
    AudioBuffer(QVector<float> interleaved, int sampleRate, int channels);

    bool isEmpty() const { return m_samples.isEmpty(); }
    int sampleRate() const { return m_sampleRate; }
    int channels() const { return m_channels; }
    qint64 frameCount() const { return m_frameCount; }

    /// Track length in seconds.
    double duration() const;

    const QVector<float> &samples() const { return m_samples; }
    qint64 sampleCount() const { return m_samples.size(); }

    qint64 frameForTime(double seconds) const;
    double timeForFrame(qint64 frame) const;

    /// Copy `count` mono-downmixed samples starting at `startFrame` into `dst`.
    ///
    /// Frames outside the track are written as silence, so a caller can centre an
    /// FFT window on any timestamp -- including the first and last few
    /// milliseconds -- without special-casing the edges.
    void readMono(qint64 startFrame, float *dst, int count) const;

    /// Min/max peak envelope for waveform drawing, one pair per bucket.
    /// Precomputed once at load so the timeline never walks the full PCM array
    /// while painting.
    void computePeaks(int bucketCount, QVector<float> &mins, QVector<float> &maxs) const;

private:
    QVector<float> m_samples;
    int m_sampleRate = 0;
    int m_channels = 0;
    qint64 m_frameCount = 0;
};

} // namespace mv

Q_DECLARE_METATYPE(mv::AudioBuffer)
