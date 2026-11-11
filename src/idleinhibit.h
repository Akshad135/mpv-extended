#pragma once

#include <QObject>
#include <QPointer>

class QWindow;
struct wl_event_queue;
struct wl_registry;
struct zwp_idle_inhibit_manager_v1;
struct zwp_idle_inhibitor_v1;

// Keeps the screen awake with the Wayland idle-inhibit protocol, the way mpv
// does in its own window. Compositor-side idle daemons (Hyprland's hypridle,
// Omarchy's shell) honour this, while many don't listen to the D-Bus portal.
// The compositor applies it only while the window is visible, which is what
// a video player wants anyway. Does nothing off Wayland or without the
// protocol.
class IdleInhibit : public QObject {
    Q_OBJECT

public:
    explicit IdleInhibit(QObject *parent = nullptr);
    ~IdleInhibit() override;

    void setWindow(QWindow *window);
    void setInhibited(bool inhibited);
    bool available() const { return m_manager; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void apply();
    void drop();

    QPointer<QWindow> m_window;
    wl_event_queue *m_queue = nullptr;
    wl_registry *m_registry = nullptr;
    zwp_idle_inhibit_manager_v1 *m_manager = nullptr;
    zwp_idle_inhibitor_v1 *m_inhibitor = nullptr;
    bool m_inhibited = false;
};
