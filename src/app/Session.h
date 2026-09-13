#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace mv {

/// What the editor was doing last time, so a relaunch picks it back up.
///
/// Deliberately separate from Project. A .mvproj is a portable template and
/// stores no audio path (see Project's header comment); the track, and where the
/// volume was left, belong to this machine's session rather than to the document.
///
/// Writes are debounced: the volume slider emits on every pixel of a drag, and one
/// file write per pixel is not worth the disk. flush() covers quit, where the
/// timer would never get to fire.
class Session : public QObject
{
    Q_OBJECT

public:
    /// Bumped when the on-disk shape changes incompatibly. A file from a version
    /// we do not know is ignored rather than guessed at -- losing a session is a
    /// minor inconvenience, reopening the wrong thing is not.
    static constexpr int kFormatVersion = 1;

    /// AppConfigLocation/session.json. Reads the organization and application
    /// names, so it must not be called before main() has set them.
    static QString defaultPath();

    explicit Session(QString path = defaultPath(), QObject *parent = nullptr);
    ~Session() override;

    /// Missing, unreadable, malformed or a version we do not know: defaults.
    void load();
    /// Write now rather than when the debounce expires. No-op when nothing changed.
    void flush();

    QString lastProject() const { return m_lastProject; }
    void setLastProject(const QString &path);

    QString lastAudio() const { return m_lastAudio; }
    void setLastAudio(const QString &path);

    qreal volume() const { return m_volume; }
    void setVolume(qreal v);

private:
    /// Marks dirty and restarts the write timer.
    void touch();
    void write();

    static constexpr int kWriteDelayMs = 500;

    QString m_path;
    QTimer m_writeTimer;
    bool m_dirty = false;

    QString m_lastProject;
    QString m_lastAudio;
    qreal m_volume = 1.0;
};

} // namespace mv
