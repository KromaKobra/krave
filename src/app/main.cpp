#include "AudioEngine.h"
#include "ExportController.h"
#include "LayerModel.h"
#include "Project.h"
#include "SpectrumEngine.h"

#include <FfmpegLocator.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Music Visualizer"));
    QGuiApplication::setOrganizationName(QStringLiteral("MusicVisualizer"));
    QGuiApplication::setApplicationVersion(QStringLiteral(MV_VERSION));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Layered, frequency-reactive music visualizer with video export."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption renderOption(
        QStringLiteral("render"),
        QStringLiteral("Render headlessly instead of opening the editor. Requires --audio "
                       "and --out."));
    const QCommandLineOption audioOption(
        QStringLiteral("audio"), QStringLiteral("Audio track to visualize."),
        QStringLiteral("file"));
    const QCommandLineOption outOption(
        QStringLiteral("out"), QStringLiteral("Output video file (.mp4 or .mov)."),
        QStringLiteral("file"));
    const QCommandLineOption projectOption(
        QStringLiteral("project"), QStringLiteral("Visualizer project to render with."),
        QStringLiteral("file"));
    const QCommandLineOption widthOption(
        QStringLiteral("width"), QStringLiteral("Output width (default 1920)."),
        QStringLiteral("px"));
    const QCommandLineOption heightOption(
        QStringLiteral("height"), QStringLiteral("Output height (default 1080)."),
        QStringLiteral("px"));
    const QCommandLineOption fpsOption(
        QStringLiteral("fps"), QStringLiteral("Output frame rate (default 30)."),
        QStringLiteral("n"));
    const QCommandLineOption noAudioOption(
        QStringLiteral("no-audio"), QStringLiteral("Export video without an audio track."));
    const QCommandLineOption codecOption(
        QStringLiteral("codec"),
        QStringLiteral("Video codec: 'h264' for MP4 (default) or 'prores' for MOV."),
        QStringLiteral("name"));
    const QCommandLineOption cancelAfterOption(
        QStringLiteral("cancel-after"),
        QStringLiteral("Testing aid: cancel the render once this many frames are done."),
        QStringLiteral("frames"));

    parser.addOptions({renderOption, audioOption, outOption, projectOption,
                       widthOption, heightOption, fpsOption, noAudioOption, codecOption,
                       cancelAfterOption});
    parser.process(app);

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

    // ---------------------------------------------------------- headless render
    //
    // Same objects, same VisualizerCanvas.qml, same export controller as the GUI
    // path -- only the editor window is skipped. Useful for batch renders, and it
    // is how the export pipeline is regression-tested.
    if (parser.isSet(renderOption)) {
        QTextStream out(stdout);
        QTextStream err(stderr);

        const QString audioPath = parser.value(audioOption);
        const QString outPath = parser.value(outOption);
        if (audioPath.isEmpty() || outPath.isEmpty()) {
            err << "--render requires both --audio and --out\n";
            return 2;
        }

        if (parser.isSet(projectOption)) {
            if (!project.load(QUrl::fromLocalFile(parser.value(projectOption)))) {
                err << "Could not load project: " << project.lastError() << "\n";
                return 2;
            }
        }

        if (parser.isSet(widthOption))
            exportController.setProperty("width", parser.value(widthOption).toInt());
        if (parser.isSet(heightOption))
            exportController.setProperty("height", parser.value(heightOption).toInt());
        if (parser.isSet(fpsOption))
            exportController.setProperty("fps", parser.value(fpsOption).toInt());
        if (parser.isSet(noAudioOption))
            exportController.setProperty("includeAudio", false);
        if (parser.isSet(codecOption)) {
            const QString codec = parser.value(codecOption);
            if (codec != QLatin1String("h264") && codec != QLatin1String("prores")) {
                err << "Unknown codec '" << codec << "'; expected 'h264' or 'prores'\n";
                return 2;
            }
            exportController.setProperty("videoCodec", codec);
        }

        int exitCode = 1;

        QObject::connect(&exportController, &mv::ExportController::finished, &app,
                         [&](bool success, const QString &path) {
                             if (success) {
                                 out << "Wrote " << path << "\n";
                                 exitCode = 0;
                             } else {
                                 err << "Export failed: " << exportController.lastError() << "\n";
                             }
                             out.flush();
                             err.flush();
                             app.quit();
                         });

        const int cancelAfter = parser.isSet(cancelAfterOption)
            ? parser.value(cancelAfterOption).toInt() : -1;

        QObject::connect(&exportController, &mv::ExportController::progressChanged, &app, [&] {
            const int done = exportController.framesDone();
            const int total = exportController.totalFrames();

            if (cancelAfter > 0 && done >= cancelAfter && exportController.isExporting()) {
                out << "\ncancelling after " << done << " frames\n";
                out.flush();
                exportController.cancel();
                return;
            }

            if (total > 0 && (done % 30 == 0 || done == total)) {
                out << "\rframe " << done << "/" << total << "   ";
                out.flush();
            }
        });

        QObject::connect(&audioEngine, &mv::AudioEngine::trackChanged, &app, [&] {
            if (!audioEngine.hasTrack())
                return;
            out << "Decoded " << audioEngine.duration() << "s of audio\n";
            out.flush();
            if (!exportController.start(QUrl::fromLocalFile(outPath))) {
                err << "Export failed to start: " << exportController.lastError() << "\n";
                app.exit(1);
            }
        });

        QObject::connect(&audioEngine, &mv::AudioEngine::errorOccurred, &app,
                         [&](const QString &message) {
                             err << "Audio error: " << message << "\n";
                             app.exit(2);
                         });

        QTimer::singleShot(0, &app, [&] { audioEngine.importFile(QUrl::fromLocalFile(audioPath)); });

        const int rc = app.exec();
        return rc != 0 ? rc : exitCode;
    }

    // ------------------------------------------------------------- editor mode
    //
    // The analysis tick runs continuously rather than only during playback, so a
    // paused track still shows the spectrum at the playhead while it is edited.
    spectrumEngine.start();

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("MusicVisualizer", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    // Opening straight onto a project and/or a track: handy from the shell, and
    // what a file association would do.
    if (parser.isSet(projectOption)) {
        if (!project.load(QUrl::fromLocalFile(parser.value(projectOption))))
            qWarning("Could not load project: %s", qPrintable(project.lastError()));
    }

    if (parser.isSet(audioOption)) {
        const QString audioPath = parser.value(audioOption);
        QTimer::singleShot(0, &app,
                           [&] { audioEngine.importFile(QUrl::fromLocalFile(audioPath)); });
    }

    return app.exec();
}
