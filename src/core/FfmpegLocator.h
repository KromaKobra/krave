#pragma once

#include <QString>

namespace mv::ffmpeg {

/// Absolute path to ffmpeg, or an empty string if it cannot be found.
///
/// Resolution order: the MV_FFMPEG environment variable, then a path previously
/// stored in QSettings, then PATH. Results are cached after the first lookup.
QString ffmpegPath();

/// Absolute path to ffprobe, resolved the same way (MV_FFPROBE, settings, PATH).
/// ffprobe is optional -- it is only used to learn a track's duration up front so
/// decode can preallocate and report real progress.
QString ffprobePath();

/// Remember an explicit ffmpeg location and clear the cache.
void setFfmpegPath(const QString &path);

bool isAvailable();

/// Clear cached lookups; mainly for tests.
void resetCache();

} // namespace mv::ffmpeg
