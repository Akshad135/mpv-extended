#include "filepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>

namespace {

// The portal's filter type, a(sa(us)): a name and (kind, pattern) rules,
// where kind 0 is a glob and 1 a MIME type.
struct FilterRule {
    uint kind = 0;
    QString pattern;
};
struct Filter {
    QString name;
    QList<FilterRule> rules;
};

} // namespace

Q_DECLARE_METATYPE(FilterRule)
Q_DECLARE_METATYPE(Filter)

QDBusArgument &operator<<(QDBusArgument &arg, const FilterRule &rule) {
    arg.beginStructure();
    arg << rule.kind << rule.pattern;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, FilterRule &rule) {
    arg.beginStructure();
    arg >> rule.kind >> rule.pattern;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const Filter &filter) {
    arg.beginStructure();
    arg << filter.name << filter.rules;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, Filter &filter) {
    arg.beginStructure();
    arg >> filter.name >> filter.rules;
    arg.endStructure();
    return arg;
}

namespace {
enum Filters { NoFilters = 0, MediaFilters = 1, SubtitleFilters = 2 };
}

FilePicker::FilePicker(QObject *parent) : QObject(parent) {
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<QList<FilterRule>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<QList<Filter>>();
}

void FilePicker::openFiles() {
    request(tr("Open Media"), "media", false, true, MediaFilters);
}

void FilePicker::openFolder() {
    request(tr("Open Folder"), "media", true, false, NoFilters);
}

void FilePicker::openSubtitle() {
    request(tr("Load Subtitles"), "subtitle", false, false, SubtitleFilters);
}

void FilePicker::request(const QString &title, const QString &purpose, bool directory, bool multiple, int filters) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    // A request the portal never answered must not block every later one:
    // close it and start fresh.
    if (busy()) {
        unsubscribe();
        bus.asyncCall(QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", m_requestPath,
                                                     "org.freedesktop.portal.Request", "Close"));
        m_requestPath.clear();
    }
    if (!bus.isConnected()) {
        emit failed(tr("No session bus, so the file picker can't open."));
        return;
    }

    // Subscribe to the response before asking, at the path the portal will
    // use, so a fast answer can't arrive before anyone is listening.
    const QString token = QString("mpv_extended%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    m_requestPath = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    m_purpose = purpose;
    bus.connect("org.freedesktop.portal.Desktop", m_requestPath, "org.freedesktop.portal.Request", "Response", this,
                SLOT(onResponse(uint, QVariantMap)));
    emit busyChanged();

    QVariantMap options{{"handle_token", token}, {"modal", true}, {"multiple", multiple}, {"directory", directory}};
    if (filters == MediaFilters) {
        const QList<Filter> list{
            {tr("Video and audio"), {{1, "video/*"}, {1, "audio/*"}}},
            {tr("All files"), {{0, "*"}}},
        };
        options.insert("filters", QVariant::fromValue(list));
    } else if (filters == SubtitleFilters) {
        const QList<Filter> list{
            {tr("Subtitles"), {{0, "*.srt"}, {0, "*.ass"}, {0, "*.ssa"}, {0, "*.vtt"}, {0, "*.sub"}, {0, "*.sup"}}},
            {tr("All files"), {{0, "*"}}},
        };
        options.insert("filters", QVariant::fromValue(list));
    }

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop",
                                                          "/org/freedesktop/portal/desktop",
                                                          "org.freedesktop.portal.FileChooser", "OpenFile");
    message << QString() << title << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    const QString expected = m_requestPath;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, expected](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        if (m_requestPath != expected)
            return; // superseded by a newer request
        QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
            unsubscribe();
            m_requestPath.clear();
            emit busyChanged();
            emit failed(tr("The file picker didn't open: %1").arg(reply.error().message()));
            return;
        }
        // Older portals pick their own handle; follow it if it differs.
        const QString actual = reply.value().path();
        if (!actual.isEmpty() && actual != m_requestPath) {
            unsubscribe();
            m_requestPath = actual;
            QDBusConnection::sessionBus().connect("org.freedesktop.portal.Desktop", m_requestPath,
                                                  "org.freedesktop.portal.Request", "Response", this,
                                                  SLOT(onResponse(uint, QVariantMap)));
        }
    });
}

void FilePicker::unsubscribe() {
    QDBusConnection::sessionBus().disconnect("org.freedesktop.portal.Desktop", m_requestPath,
                                             "org.freedesktop.portal.Request", "Response", this,
                                             SLOT(onResponse(uint, QVariantMap)));
}

void FilePicker::onResponse(uint response, const QVariantMap &results) {
    unsubscribe();
    m_requestPath.clear();
    emit busyChanged();
    // 1 is a cancel, which needs no message.
    if (response == 0) {
        const QStringList uris = results.value("uris").toStringList();
        if (!uris.isEmpty())
            emit picked(uris, m_purpose);
    } else if (response != 1) {
        emit failed(tr("The file picker closed with an error."));
    }
}
