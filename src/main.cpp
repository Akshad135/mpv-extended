// reel — a video player for Omarchy, built on libmpv.

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QTimer>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QUrl>
#include <algorithm>

#include "filepicker.h"
#include "inhibitor.h"
#include "library.h"
#include "mpvobject.h"
#include "theme.h"
#include "thumbnailer.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // mpv draws through OpenGL, so Qt Quick has to render with it too.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    app.setApplicationName("reel");
    app.setApplicationDisplayName("Reel");
    app.setApplicationVersion("0.1.0");

    const QStringList args = app.arguments().mid(1);
    if (args.contains("--help") || args.contains("-h")) {
        QTextStream(stdout) << "Usage: reel [file|folder|url ...]\n\n"
                               "Plays video and audio. A folder plays everything in it; a single file\n"
                               "plays with the other files beside it queued up next.\n";
        return 0;
    }
    if (args.contains("--version")) {
        QTextStream(stdout) << "reel " << app.applicationVersion() << "\n";
        return 0;
    }

    // Matches the window to reel.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("reel");
    app.setWindowIcon(QIcon::fromTheme("reel"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Library library;
    Thumbnailer thumbnailer;
    FilePicker picker;
    Inhibitor inhibitor;
    MpvObject::setResumeLookup([&library](const QString &path) { return library.resumePosition(path); });
    qmlRegisterType<MpvObject>("Reel", 1, 0, "MpvVideo");

    // Relative paths are resolved here, against the directory reel started in.
    QStringList inputs;
    for (const auto &arg : args) {
        if (arg.startsWith("--"))
            continue;
        const QFileInfo info(arg);
        inputs.append(info.exists() ? info.absoluteFilePath() : arg);
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("library", &library);
    engine.rootContext()->setContextProperty("thumbnailer", &thumbnailer);
    engine.rootContext()->setContextProperty("picker", &picker);
    engine.rootContext()->setContextProperty("inhibitor", &inhibitor);
    engine.rootContext()->setContextProperty("initialInputs", inputs);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Development aid: REEL_GRAB=out.png saves the window after REEL_GRAB_AFTER
    // milliseconds (and after evaluating REEL_GRAB_EVAL in the window's QML
    // scope, to set up a state), then quits. It lets the interface be checked
    // without anyone watching the screen.
    if (const QString grab = qEnvironmentVariable("REEL_GRAB"); !grab.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const int after = qEnvironmentVariableIntValue("REEL_GRAB_AFTER") ?: 2500;
        const QString script = qEnvironmentVariable("REEL_GRAB_EVAL");
        QTimer::singleShot(std::max(0, after - 700), window, [window, script] {
            if (!script.isEmpty()) {
                QQmlExpression expression(QQmlEngine::contextForObject(window), window, script);
                expression.evaluate();
                if (expression.hasError())
                    qWarning("REEL_GRAB_EVAL: %s", qPrintable(expression.error().toString()));
            }
        });
        QTimer::singleShot(after, window, [window, grab] {
            window->grabWindow().save(grab);
            QGuiApplication::quit();
        });
    }

    return app.exec();
}
