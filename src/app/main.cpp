#include "AudioEngine.h"
#include "ExportController.h"
#include "NodeGraph.h"
#include "Project.h"
#include "Session.h"
#include "SpectrumEngine.h"

#include <FfmpegLocator.h>

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStringList>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Krave"));
    QGuiApplication::setOrganizationName(QStringLiteral("kroma"));
    QGuiApplication::setApplicationVersion(QStringLiteral(MV_VERSION));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // The backend objects are owned here and handed to QML as context properties.
    // QML stays declarative: it binds to these, it does not construct them.
    mv::NodeGraph nodeGraph;
    mv::AudioEngine audioEngine;
    mv::SpectrumEngine spectrumEngine;
    mv::Project project;
    mv::ExportController exportController;

    project.setGraph(&nodeGraph);
    spectrumEngine.setGraph(&nodeGraph);
    spectrumEngine.setTimeSource([&audioEngine] { return audioEngine.position(); });

    QObject::connect(&audioEngine, &mv::AudioEngine::trackChanged, &spectrumEngine, [&] {
        spectrumEngine.setBuffer(audioEngine.buffer());
    });

    // A jump is not a continuation: drop the smoothing envelopes so the old
    // position's spectrum does not bleed into the new one.
    QObject::connect(&audioEngine, &mv::AudioEngine::seeked,
                     &spectrumEngine, &mv::SpectrumEngine::reset);

    // Same argument across a document change. Analyzers are keyed by node id, and
    // a freshly loaded file restarts those ids from the file's own numbering -- so
    // without this, node 3 of the new project inherits node 3 of the old one's
    // smoothing state for a frame.
    QObject::connect(&project, &mv::Project::loaded,
                     &spectrumEngine, &mv::SpectrumEngine::reset);

    // --------------------------------------------------------------- session
    //
    // Constructed after setApplicationName/setOrganizationName above, which are
    // what Session::defaultPath() resolves the config directory from.
    mv::Session session;
    session.load();

    // Snapshot before anything starts emitting. newProject() below clears the
    // project path, and the save hooks -- which is exactly why they are not
    // connected yet -- would write that empty value straight over what we came
    // here to restore.
    const QString lastProject = session.lastProject();
    const QString lastAudio = session.lastAudio();
    audioEngine.setVolume(session.volume());

    project.newProject();

    // Restore only what is still on disk. The existence check keeps a deleted file
    // from reaching the decoder and raising the modal error dialog on every single
    // launch; a missing file is a quiet notice instead.
    QStringList sessionWarnings;
    if (!lastProject.isEmpty()) {
        if (!QFileInfo::exists(lastProject)) {
            sessionWarnings << QObject::tr("Couldn't reopen %1 — the file has moved or been "
                                           "deleted.")
                                   .arg(QFileInfo(lastProject).fileName());
        } else if (!project.load(QUrl::fromLocalFile(lastProject))) {
            // Wrong format, bad JSON: load() has already phrased it better than
            // anything this could say.
            sessionWarnings << project.lastError();
        }
    }
    if (!lastAudio.isEmpty()) {
        if (QFileInfo::exists(lastAudio)) {
            audioEngine.importFile(QUrl::fromLocalFile(lastAudio));
        } else {
            sessionWarnings << QObject::tr("Couldn't reload %1 — the file has moved or been "
                                           "deleted.")
                                   .arg(QFileInfo(lastAudio).fileName());
        }
    }

    // Connected only now, once the restore above has read everything it needs.
    QObject::connect(&project, &mv::Project::filePathChanged, &session,
                     [&] { session.setLastProject(project.filePath()); });
    QObject::connect(&audioEngine, &mv::AudioEngine::trackChanged, &session,
                     [&] { session.setLastAudio(audioEngine.trackPath()); });
    QObject::connect(&audioEngine, &mv::AudioEngine::volumeChanged, &session,
                     [&] { session.setVolume(audioEngine.volume()); });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &session,
                     [&] { session.flush(); });

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("audioEngine"), &audioEngine);
    ctx->setContextProperty(QStringLiteral("nodeGraph"), &nodeGraph);
    ctx->setContextProperty(QStringLiteral("spectrumEngine"), &spectrumEngine);
    ctx->setContextProperty(QStringLiteral("project"), &project);
    ctx->setContextProperty(QStringLiteral("exportController"), &exportController);
    ctx->setContextProperty(QStringLiteral("ffmpegAvailable"), mv::ffmpeg::isAvailable());
    ctx->setContextProperty(QStringLiteral("ffmpegPath"), mv::ffmpeg::ffmpegPath());
    ctx->setContextProperty(QStringLiteral("sessionWarning"),
                            sessionWarnings.join(QStringLiteral(" ")));

    exportController.setContext(&engine, &nodeGraph, &audioEngine, &spectrumEngine, &project);

    // The analysis tick runs continuously rather than only during playback, so a
    // paused track still shows the spectrum at the playhead while it is edited.
    spectrumEngine.start();

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("krave", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
