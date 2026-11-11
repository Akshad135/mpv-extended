#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The whole Omarchy palette, not just the accent, so every surface in the
// player is drawn from the desktop theme and follows a switch live.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isLight READ isLight NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor raised READ raised NOTIFY changed)
    Q_PROPERTY(QColor base READ base NOTIFY changed)
    Q_PROPERTY(QColor deep READ deep NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor strong READ strong NOTIFY changed)
    Q_PROPERTY(QColor dim READ dim NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    bool isLight() const { return m_light; }
    QColor accent() const { return color("accent"); }
    QColor accentText() const;
    QColor background() const { return color("background"); }
    QColor surface() const { return color("lighter_background"); }
    QColor raised() const { return mix(surface(), foreground(), m_light ? .06 : .05); }
    QColor base() const { return color("dark_background"); }
    QColor deep() const { return color("darker_background"); }
    QColor foreground() const { return color("foreground"); }
    QColor strong() const { return color("bright_foreground"); }
    QColor dim() const { return color("dark_foreground"); }
    QColor muted() const { return color("muted"); }
    QColor selection() const { return color("selection"); }
    QColor red() const { return color("red"); }
    QColor green() const { return color("green"); }
    QColor yellow() const { return color("yellow"); }
    int radius() const { return m_radius; }

    // Colors for QML that need a tint of a palette color.
    Q_INVOKABLE QColor alpha(const QColor &color, qreal alpha) const;
    Q_INVOKABLE QColor mix(const QColor &a, const QColor &b, qreal amount) const;

    // Reads colors.toml into name -> color, and fills in what older themes
    // leave out (they only define background, foreground, and accent).
    static QHash<QString, QColor> readPalette(const QString &path, bool *light = nullptr);

    // Asks Hyprland for its rounding again. Runs at start, when Hyprland
    // reloads its config, and when the window comes back into focus.
    void refreshRounding();

signals:
    void changed();

private:
    QColor color(const QString &name) const { return m_palette.value(name); }
    void reload();

    QString m_directory;
    QHash<QString, QColor> m_palette;
    bool m_light = false;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingTimeout;
    QLocalSocket m_hyprEvents;
    QByteArray m_hyprEventBuffer;
};
