#include "AudioDecoder.h"
#include "FfmpegLocator.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFileInfo>
#include <QProcess>

namespace mv {
namespace {

/// Ask ffprobe for the track duration so decode can preallocate and report real
/// progress. Returns a negative value when ffprobe is missing or unhelpful --
/// decoding still works, it just reports indeterminate progress.
double probeDuration(const QString &filePath)
{
    const QString probe = ffmpeg::ffprobePath();
    if (probe.isEmpty())
        return -1.0;

    QProcess p;
    p.start(probe, {QStringLiteral("-v"), QStringLiteral("error"),
                    QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
                    QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
                    filePath});
    if (!p.waitForStarted(5000))
        return -1.0;
    if (!p.waitForFinished(15000)) {
        p.kill();
        p.waitForFinished(1000);
        return -1.0;
    }

    bool ok = false;
    const double seconds = QString::fromUtf8(p.readAllStandardOutput()).trimmed().toDouble(&ok);
    return (ok && seconds > 0.0) ? seconds : -1.0;
}

} // namespace

AudioDecoder::AudioDecoder(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFutureWatcher<Result>(this))
{
    qRegisterMetaType<mv::AudioBuffer>("mv::AudioBuffer");
    connect(m_watcher, &QFutureWatcher<Result>::finished, this, &AudioDecoder::onFinished);
}

AudioDecoder::~AudioDecoder()
{
    if (m_watcher->isRunning())
        m_watcher->waitForFinished();
}

void AudioDecoder::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged(m_busy);
}

bool AudioDecoder::decode(const QString &filePath)
{
    if (m_busy) {
        Q_EMIT failed(tr("A decode is already in progress."));
        return false;
    }
    if (!QFileInfo::exists(filePath)) {
        Q_EMIT failed(tr("File not found: %1").arg(filePath));
        return false;
    }
    if (!ffmpeg::isAvailable()) {
        Q_EMIT failed(tr("ffmpeg was not found. Install it or set the MV_FFMPEG "
                         "environment variable to its full path."));
        return false;
    }

    setBusy(true);
    Q_EMIT progress(0.0);

    // The lambda runs on a pool thread. It reports progress by emitting through
    // `this`, which is safe because the connection to any GUI receiver is queued.
    auto job = [this, filePath]() -> Result {
        Result result;
        result.filePath = filePath;

        const double expectedSeconds = probeDuration(filePath);
        const qint64 expectedBytes = expectedSeconds > 0.0
            ? qint64(expectedSeconds * kTargetSampleRate) * kTargetChannels * qint64(sizeof(float))
            : -1;

        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);
        proc.start(ffmpeg::ffmpegPath(), {
            QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-i"), filePath,
            QStringLiteral("-f"), QStringLiteral("f32le"),
            QStringLiteral("-acodec"), QStringLiteral("pcm_f32le"),
            QStringLiteral("-ac"), QString::number(kTargetChannels),
            QStringLiteral("-ar"), QString::number(kTargetSampleRate),
            QStringLiteral("-")
        });

        if (!proc.waitForStarted(10000)) {
            result.error = tr("Could not start ffmpeg.");
            return result;
        }

        QByteArray pcm;
        if (expectedBytes > 0)
            pcm.reserve(int(qMin<qint64>(expectedBytes, 1LL << 30)));
        QByteArray stderrText;

        // Drain stdout continuously. Letting the pipe fill would deadlock ffmpeg.
        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(100);
            pcm.append(proc.readAllStandardOutput());
            stderrText.append(proc.readAllStandardError());
            if (expectedBytes > 0)
                Q_EMIT progress(qBound(0.0, double(pcm.size()) / double(expectedBytes), 0.999));
            else
                Q_EMIT progress(-1.0);
        }
        pcm.append(proc.readAllStandardOutput());
        stderrText.append(proc.readAllStandardError());
        proc.waitForFinished(5000);

        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            const QString detail = QString::fromUtf8(stderrText).trimmed();
            result.error = detail.isEmpty()
                ? tr("ffmpeg failed with exit code %1.").arg(proc.exitCode())
                : tr("ffmpeg failed: %1").arg(detail);
            return result;
        }
        if (pcm.isEmpty()) {
            result.error = tr("No audio was decoded from %1.").arg(QFileInfo(filePath).fileName());
            return result;
        }

        const qint64 sampleCount = pcm.size() / qint64(sizeof(float));
        // Declared then resized, not constructed with a size: the one-argument
        // form is parsed as a function declaration here (most vexing parse).
        QVector<float> samples;
        samples.resize(qsizetype(sampleCount));
        std::memcpy(samples.data(), pcm.constData(), size_t(sampleCount) * sizeof(float));

        result.buffer = AudioBuffer(std::move(samples), kTargetSampleRate, kTargetChannels);
        return result;
    };

    m_watcher->setFuture(QtConcurrent::run(job));
    return true;
}

void AudioDecoder::onFinished()
{
    const Result result = m_watcher->result();
    setBusy(false);

    if (!result.error.isEmpty()) {
        Q_EMIT progress(0.0);
        Q_EMIT failed(result.error);
        return;
    }

    Q_EMIT progress(1.0);
    Q_EMIT decoded(result.buffer, result.filePath);
}

} // namespace mv
