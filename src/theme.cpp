// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <utility>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_palette = readPalette({}, &m_light);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        m_roundingTimeout.stop();
        if (error == QProcess::FailedToStart && std::exchange(m_roundingAgain, false))
            QTimer::singleShot(0, this, &Theme::refreshRounding);
    });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        // A refresh asked for while this query ran may follow a change it
        // didn't see; ask once more.
        if (std::exchange(m_roundingAgain, false))
            QTimer::singleShot(0, this, &Theme::refreshRounding);
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    // Hyprland announces config reloads (theme switches included) on its
    // event socket, so there's nothing to poll. Focus coming back covers a
    // rounding set at runtime with hyprctl keyword.
    if (!m_hyprctl.isEmpty()) {
        const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
        const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (!signature.isEmpty() && !runtime.isEmpty()) {
            connect(&m_hyprEvents, &QLocalSocket::readyRead, this, [this] {
                m_hyprEventBuffer += m_hyprEvents.readAll();
                bool reloaded = false;
                for (qsizetype end; (end = m_hyprEventBuffer.indexOf('\n')) >= 0;) {
                    reloaded |= m_hyprEventBuffer.startsWith("configreloaded>>");
                    m_hyprEventBuffer.remove(0, end + 1);
                }
                if (reloaded)
                    refreshRounding();
            });
            m_hyprEvents.connectToServer(runtime + "/hypr/" + signature + "/.socket2.sock", QIODevice::ReadOnly);
        }
        if (qGuiApp)
            connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive)
                    refreshRounding();
            });
        refreshRounding();
    }
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty())
        return;
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingAgain = true;
        return;
    }
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QColor Theme::alpha(const QColor &color, qreal alpha) const {
    QColor c = color;
    c.setAlphaF(std::clamp(alpha, 0.0, 1.0));
    return c;
}

QColor Theme::mix(const QColor &a, const QColor &b, qreal amount) const {
    const qreal t = std::clamp(amount, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QColor Theme::accentText() const {
    const QColor c = accent();
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? QColor(Qt::black) : QColor(Qt::white);
}

QHash<QString, QColor> Theme::readPalette(const QString &path, bool *light) {
    QHash<QString, QColor> p;
    bool isLight = false;
    QFile file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly)) {
        const QString text = QString::fromUtf8(file.readAll());
        static const QRegularExpression entry(R"re(^\s*([a-z_0-9]+)\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                              QRegularExpression::MultilineOption);
        for (auto it = entry.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            p.insert(m.captured(1), QColor(m.captured(2)));
        }
        static const QRegularExpression mode(R"re(^\s*mode\s*=\s*["']light["'])re", QRegularExpression::MultilineOption);
        isLight = mode.match(text).hasMatch();
    }

    // Without Omarchy, a neutral dark palette with Omarchy's default accent.
    if (!p.contains("background")) p.insert("background", QColor("#121214"));
    if (!p.contains("foreground")) p.insert("foreground", QColor("#d6d6da"));
    if (!p.contains("accent")) p.insert("accent", QColor("#FFD60A"));
    if (!path.isEmpty() && !file.isOpen())
        isLight = false;

    const QColor bg = p["background"], fg = p["foreground"], accent = p["accent"];
    const QColor away = isLight ? QColor(Qt::white) : QColor(Qt::black);
    auto mixed = [](const QColor &a, const QColor &b, qreal t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    };
    auto fill = [&](const char *name, const QColor &value) {
        if (!p.contains(name)) p.insert(name, value);
    };
    fill("lighter_background", mixed(bg, fg, .08));
    fill("dark_background", mixed(bg, away, .25));
    fill("darker_background", mixed(bg, away, .45));
    fill("bright_foreground", mixed(fg, isLight ? QColor(Qt::black) : QColor(Qt::white), .3));
    fill("dark_foreground", mixed(fg, bg, .45));
    fill("muted", mixed(bg, fg, .28));
    fill("selection", mixed(bg, accent, .25));
    fill("red", p.value("color1", QColor("#f7768e")));
    fill("green", p.value("color2", QColor("#9ece6a")));
    fill("yellow", p.value("color3", QColor("#e0af68")));
    if (light)
        *light = isLight;
    return p;
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    bool light = false;
    const auto palette = readPalette(m_directory + "/theme/colors.toml", &light);
    if (palette != m_palette || light != m_light) {
        m_palette = palette;
        m_light = light;
        emit changed();
    }
}
