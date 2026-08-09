#include "FfmpegLocator.h"

#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace mv::ffmpeg {
namespace {

constexpr auto kSettingsKey = "tools/ffmpegPath";

QString g_ffmpeg;
QString g_ffprobe;
bool g_resolved = false;

QString validateOrEmpty(const QString &candidate)
{
    if (candidate.isEmpty())
        return {};
    const QFileInfo fi(candidate);
    if (fi.exists() && fi.isFile() && fi.isExecutable())
        return fi.absoluteFilePath();
    return {};
}

/// Given a known ffmpeg path, guess its sibling ffprobe -- they ship together.
QString siblingTool(const QString &ffmpegExe, const QString &toolName)
{
    if (ffmpegExe.isEmpty())
        return {};
    const QFileInfo fi(ffmpegExe);
    const QString suffix = fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix();
    return validateOrEmpty(fi.absolutePath() + QLatin1Char('/') + toolName + suffix);
}

void resolve()
{
    if (g_resolved)
        return;
    g_resolved = true;

    g_ffmpeg = validateOrEmpty(qEnvironmentVariable("MV_FFMPEG"));
    if (g_ffmpeg.isEmpty())
        g_ffmpeg = validateOrEmpty(QSettings().value(kSettingsKey).toString());
    if (g_ffmpeg.isEmpty())
        g_ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));

    g_ffprobe = validateOrEmpty(qEnvironmentVariable("MV_FFPROBE"));
    if (g_ffprobe.isEmpty())
        g_ffprobe = siblingTool(g_ffmpeg, QStringLiteral("ffprobe"));
    if (g_ffprobe.isEmpty())
        g_ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
}

} // namespace

QString ffmpegPath()
{
    resolve();
    return g_ffmpeg;
}

QString ffprobePath()
{
    resolve();
    return g_ffprobe;
}

void setFfmpegPath(const QString &path)
{
    QSettings().setValue(kSettingsKey, path);
    resetCache();
}

bool isAvailable()
{
    return !ffmpegPath().isEmpty();
}

void resetCache()
{
    g_resolved = false;
    g_ffmpeg.clear();
    g_ffprobe.clear();
}

} // namespace mv::ffmpeg
