#include "AudioEngine.h"
#include "ExportController.h"
#include "NodeGraph.h"
#include "Project.h"
#include "SpectrumEngine.h"

#include <FfmpegLocator.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

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

    project.newProject();

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("audioEngine"), &audioEngine);
    ctx->setContextProperty(QStringLiteral("nodeGraph"), &nodeGraph);
    ctx->setContextProperty(QStringLiteral("spectrumEngine"), &spectrumEngine);
    ctx->setContextProperty(QStringLiteral("project"), &project);
    ctx->setContextProperty(QStringLiteral("exportController"), &exportController);
    ctx->setContextProperty(QStringLiteral("ffmpegAvailable"), mv::ffmpeg::isAvailable());
    ctx->setContextProperty(QStringLiteral("ffmpegPath"), mv::ffmpeg::ffmpegPath());

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
