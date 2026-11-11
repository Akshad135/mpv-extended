#include "idleinhibit.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <cstring>
#include <wayland-client.h>

#include "idle-inhibit-unstable-v1-client-protocol.h"

namespace {

void global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t) {
    auto **manager = static_cast<zwp_idle_inhibit_manager_v1 **>(data);
    if (std::strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0 && !*manager)
        *manager = static_cast<zwp_idle_inhibit_manager_v1 *>(
            wl_registry_bind(registry, name, &zwp_idle_inhibit_manager_v1_interface, 1));
}
void globalRemove(void *, wl_registry *, uint32_t) {}
const wl_registry_listener registryListener{global, globalRemove};

} // namespace

IdleInhibit::IdleInhibit(QObject *parent) : QObject(parent) {
    auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    wl_display *display = wayland ? wayland->display() : nullptr;
    if (!display)
        return;
    // A queue of our own, so the one round trip below never dispatches
    // events meant for Qt.
    m_queue = wl_display_create_queue(display);
    auto *wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), m_queue);
    m_registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    wl_registry_add_listener(m_registry, &registryListener, &m_manager);
    wl_display_roundtrip_queue(display, m_queue);
    if (!m_manager)
        qWarning("mpv-extended: the compositor has no idle-inhibit protocol; the screen may sleep during video");
}

IdleInhibit::~IdleInhibit() {
    drop();
    if (m_manager)
        zwp_idle_inhibit_manager_v1_destroy(m_manager);
    if (m_registry)
        wl_registry_destroy(m_registry);
    if (m_queue) {
        if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
            wl_display_flush(wayland->display());
        wl_event_queue_destroy(m_queue);
    }
}

void IdleInhibit::setWindow(QWindow *window) {
    if (m_window == window)
        return;
    drop();
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (m_window)
        m_window->installEventFilter(this);
    apply();
}

void IdleInhibit::setInhibited(bool inhibited) {
    m_inhibited = inhibited;
    apply();
}

bool IdleInhibit::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_window) {
        // An inhibitor belongs to one wl_surface, which Qt replaces when the
        // window is hidden and shown again; it goes before the surface does.
        switch (event->type()) {
        case QEvent::Hide:
            drop();
            break;
        case QEvent::PlatformSurface:
            if (static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() ==
                QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                drop();
            break;
        case QEvent::Show:
        case QEvent::Expose:
            apply();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void IdleInhibit::apply() {
    if (!m_manager)
        return;
    if (!m_inhibited || !m_window || !m_window->isVisible()) {
        drop();
        return;
    }
    if (m_inhibitor)
        return;
    auto *native = QGuiApplication::platformNativeInterface();
    auto *surface = native ? static_cast<wl_surface *>(native->nativeResourceForWindow("surface", m_window)) : nullptr;
    if (!surface)
        return; // Not mapped yet; the next Expose tries again.
    m_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(m_manager, surface);
    if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
        wl_display_flush(wayland->display());
}

void IdleInhibit::drop() {
    if (!m_inhibitor)
        return;
    zwp_idle_inhibitor_v1_destroy(m_inhibitor);
    m_inhibitor = nullptr;
    if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
        wl_display_flush(wayland->display());
}
