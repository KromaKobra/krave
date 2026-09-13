#include "Session.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace mv;

namespace {

bool writeRaw(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(bytes) == bytes.size();
}

} // namespace

class TestSession : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip();
    void missingFileLoadsDefaults();
    void malformedFileLoadsDefaults();
    void unknownVersionIsIgnored();
    void loadIsNotAnEdit();
    void volumeIsClamped();
};

/// The whole point of the class: what was set last run comes back this run.
void TestSession::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    {
        Session session(path);
        session.setLastProject(QStringLiteral("/projects/skyline.mvproj"));
        session.setLastAudio(QStringLiteral("/music/track.mp3"));
        session.setVolume(0.35);
        session.flush();
    }

    QVERIFY(QFile::exists(path));

    Session restored(path);
    restored.load();
    QCOMPARE(restored.lastProject(), QStringLiteral("/projects/skyline.mvproj"));
    QCOMPARE(restored.lastAudio(), QStringLiteral("/music/track.mp3"));
    QCOMPARE(restored.volume(), 0.35);
}

/// First run. No file is not an error.
void TestSession::missingFileLoadsDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Session session(dir.filePath(QStringLiteral("nothing-here.json")));
    session.load();
    QVERIFY(session.lastProject().isEmpty());
    QVERIFY(session.lastAudio().isEmpty());
    QCOMPARE(session.volume(), 1.0);
}

void TestSession::malformedFileLoadsDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));
    QVERIFY(writeRaw(path, QByteArrayLiteral("{ not json at all")));

    Session session(path);
    session.load();
    QVERIFY(session.lastProject().isEmpty());
    QCOMPARE(session.volume(), 1.0);
}

/// A file from a version we do not know is ignored rather than guessed at.
void TestSession::unknownVersionIsIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    QJsonObject root;
    root.insert(QStringLiteral("version"), Session::kFormatVersion + 1);
    root.insert(QStringLiteral("lastProject"), QStringLiteral("/projects/future.mvproj"));
    root.insert(QStringLiteral("volume"), 0.2);
    QVERIFY(writeRaw(path, QJsonDocument(root).toJson()));

    Session session(path);
    session.load();
    QVERIFY(session.lastProject().isEmpty());
    QCOMPARE(session.volume(), 1.0);
}

/// load() must not leave the session dirty, or a launch that restores and then
/// quits would rewrite a file nothing actually changed.
void TestSession::loadIsNotAnEdit()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    {
        Session session(path);
        session.setLastAudio(QStringLiteral("/music/track.mp3"));
        session.flush();
    }
    Session session(path);
    session.load();

    // Removing the file makes the assertion exact: a flush with nothing to say
    // writes nothing at all, so the file stays gone.
    QVERIFY(QFile::remove(path));
    session.flush();
    QVERIFY(!QFile::exists(path));
}

void TestSession::volumeIsClamped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Session session(dir.filePath(QStringLiteral("session.json")));
    session.setVolume(4.0);
    QCOMPARE(session.volume(), 1.0);
    session.setVolume(-1.0);
    QCOMPARE(session.volume(), 0.0);
}

// No GUI anywhere in Session -- unlike tst_project and tst_graph this needs no
// QGuiApplication, and so no offscreen platform plugin either.
QTEST_GUILESS_MAIN(TestSession)
#include "tst_session.moc"
