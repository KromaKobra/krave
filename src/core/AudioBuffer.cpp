#include "AudioBuffer.h"

#include <algorithm>
#include <cmath>

namespace mv {

AudioBuffer::AudioBuffer(QVector<float> interleaved, int sampleRate, int channels)
    : m_samples(std::move(interleaved))
    , m_sampleRate(sampleRate)
    , m_channels(std::max(1, channels))
{
    m_frameCount = m_samples.size() / m_channels;
}

double AudioBuffer::duration() const
{
    if (m_sampleRate <= 0)
        return 0.0;
    return double(m_frameCount) / double(m_sampleRate);
}

qint64 AudioBuffer::frameForTime(double seconds) const
{
    if (m_sampleRate <= 0)
        return 0;
    return qint64(std::llround(seconds * double(m_sampleRate)));
}

double AudioBuffer::timeForFrame(qint64 frame) const
{
    if (m_sampleRate <= 0)
        return 0.0;
    return double(frame) / double(m_sampleRate);
}

void AudioBuffer::readMono(qint64 startFrame, float *dst, int count) const
{
    if (!dst || count <= 0)
        return;

    if (m_samples.isEmpty() || m_channels <= 0) {
        std::fill(dst, dst + count, 0.0f);
        return;
    }

    const float *src = m_samples.constData();
    const float inv = 1.0f / float(m_channels);

    for (int i = 0; i < count; ++i) {
        const qint64 frame = startFrame + i;
        if (frame < 0 || frame >= m_frameCount) {
            dst[i] = 0.0f;
            continue;
        }
        const qint64 base = frame * m_channels;
        float sum = 0.0f;
        for (int c = 0; c < m_channels; ++c)
            sum += src[base + c];
        dst[i] = sum * inv;
    }
}

void AudioBuffer::computePeaks(int bucketCount, QVector<float> &mins, QVector<float> &maxs) const
{
    mins.assign(std::max(0, bucketCount), 0.0f);
    maxs.assign(std::max(0, bucketCount), 0.0f);

    if (bucketCount <= 0 || m_frameCount <= 0 || m_channels <= 0)
        return;

    const float *src = m_samples.constData();

    for (int b = 0; b < bucketCount; ++b) {
        const qint64 from = (m_frameCount * b) / bucketCount;
        qint64 to = (m_frameCount * (b + 1)) / bucketCount;
        if (to <= from)
            to = from + 1;
        to = std::min(to, m_frameCount);

        float lo = 0.0f;
        float hi = 0.0f;
        bool first = true;

        for (qint64 f = from; f < to; ++f) {
            const qint64 base = f * m_channels;
            float sum = 0.0f;
            for (int c = 0; c < m_channels; ++c)
                sum += src[base + c];
            const float v = sum / float(m_channels);
            if (first) {
                lo = hi = v;
                first = false;
            } else {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
        }

        mins[b] = lo;
        maxs[b] = hi;
    }
}

} // namespace mv
