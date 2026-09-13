#include "Session.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace mv {

QString Session::defaultPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        return {};
    return dir + QStringLiteral("/session.json");
}

Session::Session(QString path, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
{
    m_writeTimer.setSingleShot(true);
    m_writeTimer.setInterval(kWriteDelayMs);
    connect(&m_writeTimer, &QTimer::timeout, this, &Session::write);
}

Session::~Session()
{
    // Last resort. main() flushes on aboutToQuit, which is the path that actually
    // matters; this catches a Session torn down some other way.
    flush();
}

void Session::load()
{
    if (m_path.isEmpty())
        return;

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return; // first run

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kFormatVersion)
        return;

    m_lastProject = root.value(QStringLiteral("lastProject")).toString();
    m_lastAudio = root.value(QStringLiteral("lastAudio")).toString();
    m_volume = qBound(0.0, root.value(QStringLiteral("volume")).toDouble(1.0), 1.0);

    // Loading is not an edit. Nothing to write back.
    m_dirty = false;
}

void Session::touch()
{
    m_dirty = true;
    m_writeTimer.start();
}

void Session::flush()
{
    m_writeTimer.stop();
    if (m_dirty)
        write();
}

void Session::write()
{
    m_dirty = false;
    if (m_path.isEmpty())
        return;

    // On a first run the config directory does not exist yet, and QSaveFile will
    // not create it.
    const QDir dir = QFileInfo(m_path).dir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath()))
        return;

    QJsonObject root;
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("lastProject"), m_lastProject);
    root.insert(QStringLiteral("lastAudio"), m_lastAudio);
    root.insert(QStringLiteral("volume"), m_volume);

    // QSaveFile, as Project does: a failed write leaves the previous session
    // intact rather than truncating it.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void Session::setLastProject(const QString &path)
{
    if (m_lastProject == path)
        return;
    m_lastProject = path;
    touch();
}

void Session::setLastAudio(const QString &path)
{
    if (m_lastAudio == path)
        return;
    m_lastAudio = path;
    touch();
}

void Session::setVolume(qreal v)
{
    const qreal clamped = qBound(qreal(0), v, qreal(1));
    if (qFuzzyCompare(m_volume, clamped))
        return;
    m_volume = clamped;
    touch();
}

} // namespace mv
