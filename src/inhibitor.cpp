#include "inhibitor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

void Inhibitor::setActive(bool active) {
    if (active == m_active)
        return;
    m_active = active;
    emit activeChanged();

    if (!active) {
        release();
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop",
                                                          "/org/freedesktop/portal/desktop",
                                                          "org.freedesktop.portal.Inhibit", "Inhibit");
    // 8 is "idle": no dimming, locking, or sleeping for idleness.
    message << QString() << uint(8) << QVariantMap{{"reason", tr("Playing video")}};
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
            qWarning("reel: can't keep the screen awake: %s", qPrintable(reply.error().message()));
            return;
        }
        m_handle = reply.value().path();
        // Playback may have paused while the portal was answering.
        if (!m_active)
            release();
    });
}

void Inhibitor::release() {
    if (m_handle.isEmpty())
        return;
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop", m_handle, "org.freedesktop.portal.Request", "Close"));
    m_handle.clear();
}
