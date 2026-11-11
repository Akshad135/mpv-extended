// mpv-extended — a video player for Omarchy, built on libmpv.

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
#include <QSocketNotifier>
#include <QUrl>
#include <algorithm>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "filepicker.h"
#include "inhibitor.h"
#include "library.h"
#include "mpvobject.h"
#include "theme.h"
#include "thumbnailer.h"
#include "translator.h"

namespace {
// Written by the signal handler, read by the event loop.
int signalPipe[2] = {-1, -1};
} // namespace

int main(int argc, char *argv[]) {
    // mpv draws through OpenGL, so Qt Quick has to render with it too.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    app.setApplicationName("mpv-extended");
    app.setApplicationDisplayName("mpv-extended");
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    const QStringList args = app.arguments().mid(1);
    if (args.contains("--help") || args.contains("-h")) {
        QTextStream(stdout) << "Usage: mpv-extended [file|folder|url ...]\n\n"
                               "Plays video and audio. A folder plays everything in it; a single file\n"
                               "plays with the other files beside it queued up next.\n";
        return 0;
    }
    if (args.contains("--version")) {
        QTextStream(stdout) << "mpv-extended " << app.applicationVersion() << "\n";
        return 0;
    }

    // Matches the window to mpv-extended.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("mpv-extended");
    app.setWindowIcon(QIcon::fromTheme("mpv-extended"));

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
    qmlRegisterType<MpvObject>("MpvExtended", 1, 0, "MpvVideo");

    // Relative paths are resolved here, against the directory it started in.
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
    auto *window = qobject_cast<QWindow *>(engine.rootObjects().first());
    inhibitor.setWindow(window);

    // Logging out or `kill` closes the window properly, so the position is
    // saved just as when the window is closed by hand.
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, signalPipe) == 0) {
        struct sigaction action {};
        action.sa_handler = [](int) {
            const char byte = 1;
            [[maybe_unused]] auto written = ::write(signalPipe[0], &byte, 1);
        };
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        for (int signal : {SIGTERM, SIGINT, SIGHUP})
            sigaction(signal, &action, nullptr);
        auto *notifier = new QSocketNotifier(signalPipe[1], QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, &app, [notifier, window] {
            notifier->setEnabled(false);
            if (window)
                window->close();
            // In case closing is held up, quit regardless after a moment.
            QTimer::singleShot(3000, qApp, &QCoreApplication::quit);
        });
    }

#ifdef MPV_EXTENDED_DEV
    // Development aid, only in `bin/build CONFIG+=dev` builds: MPV_EXTENDED_GRAB=out.png saves the window after MPV_EXTENDED_GRAB_AFTER
    // milliseconds (and after evaluating MPV_EXTENDED_GRAB_EVAL in the window's QML
    // scope, to set up a state), then quits. It lets the interface be checked
    // without anyone watching the screen.
    if (const QString grab = qEnvironmentVariable("MPV_EXTENDED_GRAB"); !grab.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window) {
            qWarning("MPV_EXTENDED_GRAB: the root object isn't a window");
            return 1;
        }
        const int after = qEnvironmentVariableIntValue("MPV_EXTENDED_GRAB_AFTER") ?: 2500;
        const QString script = qEnvironmentVariable("MPV_EXTENDED_GRAB_EVAL");
        QTimer::singleShot(std::max(0, after - 700), window, [window, script] {
            if (!script.isEmpty()) {
                QQmlExpression expression(QQmlEngine::contextForObject(window), window, script);
                expression.evaluate();
                if (expression.hasError())
                    qWarning("MPV_EXTENDED_GRAB_EVAL: %s", qPrintable(expression.error().toString()));
            }
        });
        QTimer::singleShot(after, window, [window, grab] {
            if (!window->grabWindow().save(grab))
                qWarning("MPV_EXTENDED_GRAB: couldn't save %s", qPrintable(grab));
            QGuiApplication::quit();
        });
    }
#endif

    return app.exec();
}
