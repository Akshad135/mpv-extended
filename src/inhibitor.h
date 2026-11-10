#pragma once

#include <QObject>

// Keeps the screen awake while a video plays, through xdg-desktop-portal's
// Inhibit, and lets go the moment playback pauses or stops.
class Inhibitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit Inhibitor(QObject *parent = nullptr) : QObject(parent) {}
    ~Inhibitor() override { release(); }

    bool active() const { return m_active; }
    void setActive(bool active);

signals:
    void activeChanged();

private:
    void release();
    static void close(const QString &handle);

    bool m_active = false;
    QString m_handle;
};
