#pragma once

#include <QObject>

#include "idleinhibit.h"

class QWindow;

// Keeps the screen awake while a video plays, and lets go the moment
// playback pauses or stops. Two ways at once, since desktops honour
// different ones: Wayland idle-inhibit on the window (Hyprland, Omarchy,
// wlroots), and xdg-desktop-portal's Inhibit (GNOME, KDE).
class Inhibitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit Inhibitor(QObject *parent = nullptr) : QObject(parent) {}
    ~Inhibitor() override { release(); }

    bool active() const { return m_active; }
    void setActive(bool active);
    void setWindow(QWindow *window) { m_wayland.setWindow(window); }

signals:
    void activeChanged();

private:
    void release();
    static void close(const QString &handle);

    IdleInhibit m_wayland;
    bool m_active = false;
    QString m_handle;
};
