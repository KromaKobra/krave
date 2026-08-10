#include "AudioEngine.h"
#include "ExportController.h"
#include "LayerModel.h"
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
    mv::LayerModel layerModel;
    mv::AudioEngine audioEngine;
    mv::SpectrumEngine spectrumEngine;
    mv::Project project;
    mv::ExportController exportController;

    project.setLayerModel(&layerModel);
    spectrumEngine.setLayerModel(&layerModel);
    spectrumEngine.setTimeSource([&audioEngine] { return audioEngine.position(); });

    QObject::connect(&audioEngine, &mv::AudioEngine::trackChanged, &spectrumEngine, [&] {
        spectrumEngine.setBuffer(audioEngine.buffer());
    });

    // A jump is not a continuation: drop the smoothing envelopes so the old
    // position's spectrum does not bleed into the new one.
    QObject::connect(&audioEngine, &mv::AudioEngine::seeked,
                     &spectrumEngine, &mv::SpectrumEngine::reset);

    project.newProject();

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("audioEngine"), &audioEngine);
    ctx->setContextProperty(QStringLiteral("layerModel"), &layerModel);
    ctx->setContextProperty(QStringLiteral("spectrumEngine"), &spectrumEngine);
    ctx->setContextProperty(QStringLiteral("project"), &project);
    ctx->setContextProperty(QStringLiteral("exportController"), &exportController);
    ctx->setContextProperty(QStringLiteral("ffmpegAvailable"), mv::ffmpeg::isAvailable());
    ctx->setContextProperty(QStringLiteral("ffmpegPath"), mv::ffmpeg::ffmpegPath());

    exportController.setContext(&engine, &layerModel, &audioEngine, &spectrumEngine, &project);

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
