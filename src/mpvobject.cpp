// The render setup follows mpv's own Qt Quick example (mpv-player/mpv-examples).
#include "mpvobject.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QOpenGLFunctions>
#include <clocale>
#include <cmath>
#include <mutex>
#include <utility>

namespace {

std::function<double(const QString &)> &resumeLookup() {
    static std::function<double(const QString &)> lookup;
    return lookup;
}

QVariant fromNode(const mpv_node *node) {
    switch (node->format) {
    case MPV_FORMAT_STRING:
    case MPV_FORMAT_OSD_STRING:
        return QString::fromUtf8(node->u.string);
    case MPV_FORMAT_FLAG:
        return bool(node->u.flag);
    case MPV_FORMAT_INT64:
        return qlonglong(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node->u.list->num; ++i)
            list.append(fromNode(&node->u.list->values[i]));
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node->u.list->num; ++i)
            map.insert(QString::fromUtf8(node->u.list->keys[i]), fromNode(&node->u.list->values[i]));
        return map;
    }
    default:
        return {};
    }
}

QByteArray toArgument(const QVariant &value) {
    if (value.typeId() == QMetaType::Bool)
        return value.toBool() ? "yes" : "no";
    if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
        // QML hands every number over as a double, and mpv rejects "2.000000"
        // for integer properties such as aid, sid, and playlist-pos.
        const double d = value.toDouble();
        if (std::isfinite(d) && std::floor(d) == d && std::abs(d) < 1e15)
            return QByteArray::number(qint64(d));
        return QByteArray::number(d, 'f', 6);
    }
    return value.toString().toUtf8();
}

// Runs an mpv command given as strings; the array mpv wants is built here.
int runCommand(mpv_handle *mpv, const QList<QByteArray> &args, quint64 reply = 0) {
    QList<const char *> pointers;
    for (const auto &arg : args)
        pointers.append(arg.constData());
    pointers.append(nullptr);
    return reply ? mpv_command_async(mpv, reply, pointers.data()) : mpv_command(mpv, pointers.data());
}

void *procAddress(void *, const char *name) {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(QByteArray(name))) : nullptr;
}

// Every property the interface shows, observed as nodes so one conversion
// covers numbers, strings, and the track, chapter, and playlist lists.
const char *const observed[] = {
    "pause", "time-pos", "duration", "demuxer-cache-time", "volume", "mute", "speed",
    "media-title", "path", "track-list", "chapter-list", "chapter", "playlist", "playlist-pos",
    "idle-active", "eof-reached", "paused-for-cache", "sub-visibility", "metadata",
};

} // namespace

// Shared by the item (GUI thread) and its renderer (render thread). The mpv
// core lives until both are gone, so the render context can always be freed
// first, on the render thread, while its GL context is current, as render.h
// requires. The item pointer is cleared when the item dies, so mpv's update
// callback never reaches a deleted object.
struct MpvCore {
    mpv_handle *handle = nullptr;
    std::mutex mutex;
    MpvObject *item = nullptr;

    ~MpvCore() {
        if (handle)
            mpv_terminate_destroy(handle);
    }

    void post(void (MpvObject::*method)()) {
        std::lock_guard lock(mutex);
        if (item)
            QMetaObject::invokeMethod(item, method, Qt::QueuedConnection);
    }
};

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(std::shared_ptr<MpvCore> core) : m_core(std::move(core)) {}

    ~MpvRenderer() override {
        if (m_render)
            mpv_render_context_free(m_render);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        if (!m_tried) {
            // Tried once: a failure is reported, not retried on every resize.
            m_tried = true;
            mpv_opengl_init_params gl{procAddress, nullptr};
            // No MPV_RENDER_PARAM_ADVANCED_CONTROL: with it, mpv queues work
            // (screenshots, buffer allocation) for this thread and waits on
            // it forever unless mpv_render_context_update() is called after
            // every update callback. Without it, mpv's waits time out.
            QList<mpv_render_param> params{
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
            };
            // Hardware decoding hands frames over without a copy only when
            // mpv knows the Wayland display it is drawing for.
            if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
                params.append({MPV_RENDER_PARAM_WL_DISPLAY, wayland->display()});
            params.append({MPV_RENDER_PARAM_INVALID, nullptr});
            if (mpv_render_context_create(&m_render, m_core->handle, params.data()) < 0) {
                m_render = nullptr;
                qWarning("reel: could not create the mpv render context");
                m_core->post(&MpvObject::renderFailed);
            } else {
                mpv_render_context_set_update_callback(
                    m_render, [](void *core) { static_cast<MpvCore *>(core)->post(&MpvObject::requestFrame); },
                    m_core.get());
                // Files asked for before this point wait for it, since mpv
                // can't open video output without a render context.
                m_core->post(&MpvObject::renderReady);
            }
        }
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override {
        QOpenGLFramebufferObject *fbo = framebufferObject();
        if (!m_render) {
            QOpenGLFunctions *gl = QOpenGLContext::currentContext()->functions();
            gl->glClearColor(0, 0, 0, 1);
            gl->glClear(GL_COLOR_BUFFER_BIT);
            return;
        }
        mpv_opengl_fbo target{static_cast<int>(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flip = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(m_render, params);
    }

private:
    std::shared_ptr<MpvCore> m_core;
    mpv_render_context *m_render = nullptr;
    bool m_tried = false;
};

void MpvObject::setResumeLookup(std::function<double(const QString &)> lookup) {
    resumeLookup() = std::move(lookup);
}

MpvObject::MpvObject(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    // libmpv parses numbers with the C locale and refuses to start otherwise.
    std::setlocale(LC_NUMERIC, "C");

    m_core = std::make_shared<MpvCore>();
    m_core->handle = m_mpv = mpv_create();
    m_core->item = this;
    if (!m_mpv)
        qFatal("reel: could not create mpv");

    const QList<QPair<const char *, const char *>> options{
        {"vo", "libmpv"},
        {"hwdec", "auto-safe"},
        {"idle", "yes"},
        // Hold the last frame at the end so the player can offer what's next
        // instead of jumping into the following file on its own.
        {"keep-open", "always"},
        {"input-default-bindings", "no"},
        {"input-vo-keyboard", "no"},
        {"osc", "no"},
        {"terminal", "no"},
        {"ytdl", "yes"},
        {"volume-max", "150"},
        {"sub-auto", "fuzzy"},
        {"sub-font-size", "40"},
        {"sub-border-size", "2"},
        {"sub-shadow-offset", "1"},
        {"sub-color", "#FFFFFFFF"},
        {"sub-border-color", "#CC000000"},
        {"screenshot-format", "jpg"},
        {"screenshot-jpeg-quality", "88"},
        {"audio-display", "embedded-first"},
        {"demuxer-max-bytes", "150MiB"},
    };
    for (const auto &[name, value] : options)
        mpv_set_option_string(m_mpv, name, value);
    mpv_request_log_messages(m_mpv, "warn");

    if (mpv_initialize(m_mpv) < 0)
        qFatal("reel: could not initialize mpv");

    for (const char *name : observed)
        mpv_observe_property(m_mpv, 0, name, MPV_FORMAT_NODE);
    mpv_hook_add(m_mpv, 0, "on_load", 50);

    mpv_set_wakeup_callback(
        m_mpv,
        [](void *context) {
            QMetaObject::invokeMethod(static_cast<MpvObject *>(context), &MpvObject::processEvents,
                                      Qt::QueuedConnection);
        },
        this);
}

MpvObject::~MpvObject() {
    {
        std::lock_guard lock(m_core->mutex);
        m_core->item = nullptr;
    }
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    // Silence now; the core itself goes once the renderer has let go too.
    runCommand(m_mpv, {"stop"});
}

QQuickFramebufferObject::Renderer *MpvObject::createRenderer() const {
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(m_core);
}

void MpvObject::requestFrame() {
    update();
}

void MpvObject::processEvents() {
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handleEvent(event);
    }
}

void MpvObject::handleEvent(mpv_event *event) {
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto *property = static_cast<mpv_event_property *>(event->data);
        const QString name = QString::fromUtf8(property->name);
        const QVariant value = property->format == MPV_FORMAT_NODE
                                   ? fromNode(static_cast<mpv_node *>(property->data)) : QVariant();
        if (m_values.value(name) == value && m_values.contains(name))
            return;
        m_values.insert(name, value);

        static const QHash<QString, void (MpvObject::*)()> signalsByName{
            {"pause", &MpvObject::pausedChanged},
            {"time-pos", &MpvObject::positionChanged},
            {"duration", &MpvObject::durationChanged},
            {"demuxer-cache-time", &MpvObject::cacheEndChanged},
            {"volume", &MpvObject::volumeChanged},
            {"mute", &MpvObject::mutedChanged},
            {"speed", &MpvObject::speedChanged},
            {"media-title", &MpvObject::titleChanged},
            {"path", &MpvObject::pathChanged},
            {"track-list", &MpvObject::tracksChanged},
            {"chapter-list", &MpvObject::chaptersChanged},
            {"chapter", &MpvObject::chapterChanged},
            {"playlist", &MpvObject::playlistChanged},
            {"playlist-pos", &MpvObject::playlistPosChanged},
            {"idle-active", &MpvObject::idleChanged},
            {"eof-reached", &MpvObject::eofChanged},
            {"paused-for-cache", &MpvObject::bufferingChanged},
            {"sub-visibility", &MpvObject::subtitlesVisibleChanged},
            {"metadata", &MpvObject::metadataChanged},
        };
        if (auto signal = signalsByName.value(name))
            (this->*signal)();
        break;
    }
    case MPV_EVENT_HOOK: {
        auto *hook = static_cast<mpv_event_hook *>(event->data);
        if (qstrcmp(hook->name, "on_load") == 0) {
            char *raw = mpv_get_property_string(m_mpv, "path");
            m_loadingPath = raw ? QString::fromUtf8(raw) : QString();
            mpv_free(raw);
            const double start = resumeLookup() ? resumeLookup()(m_loadingPath) : 0;
            if (start > 0) {
                const QByteArray value = QByteArray::number(start, 'f', 3);
                mpv_set_property_string(m_mpv, "file-local-options/start", value.constData());
                emit resumed(start);
            }
        }
        mpv_hook_continue(m_mpv, hook->id);
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        emit fileLoaded();
        break;
    case MPV_EVENT_END_FILE: {
        auto *end = static_cast<mpv_event_end_file *>(event->data);
        if (end->reason == MPV_END_FILE_REASON_ERROR)
            emit failed(m_loadingPath, QString::fromUtf8(mpv_error_string(end->error)));
        break;
    }
    case MPV_EVENT_COMMAND_REPLY: {
        if (m_screenshots.contains(event->reply_userdata))
            emit screenshotFinished(m_screenshots.take(event->reply_userdata), event->error >= 0);
        break;
    }
    case MPV_EVENT_LOG_MESSAGE: {
        auto *message = static_cast<mpv_event_log_message *>(event->data);
        qWarning("mpv[%s] %s: %s", message->prefix, message->level, QByteArray(message->text).trimmed().constData());
        break;
    }
    default:
        break;
    }
}

void MpvObject::renderFailed() {
    m_renderFailed = true;
    emit videoOutputFailed();
    if (!m_pendingFiles.isEmpty())
        loadFiles(std::exchange(m_pendingFiles, {}), m_pendingIndex);
}

void MpvObject::renderReady() {
    m_ready = true;
    if (!m_pendingFiles.isEmpty())
        loadFiles(std::exchange(m_pendingFiles, {}), m_pendingIndex);
}

void MpvObject::loadFiles(const QStringList &files, int index) {
    if (files.isEmpty())
        return;
    if (m_renderFailed) {
        emit failed(files.value(index), tr("There's no OpenGL video output, so video can't be shown."));
        return;
    }
    if (!m_ready) {
        m_pendingFiles = files;
        m_pendingIndex = index;
        return;
    }
    runCommand(m_mpv, {"stop"});
    for (const auto &file : files)
        runCommand(m_mpv, {"loadfile", file.toUtf8(), "append"});
    runCommand(m_mpv, {"playlist-play-index", QByteArray::number(qBound(0, index, int(files.size()) - 1))});
    mpv_set_property_string(m_mpv, "pause", "no");
}

bool MpvObject::command(const QVariantList &args) {
    QList<QByteArray> strings;
    for (const auto &arg : args)
        strings.append(toArgument(arg));
    const int error = runCommand(m_mpv, strings);
    if (error < 0)
        qWarning("reel: mpv command %s failed: %s", strings.value(0).constData(), mpv_error_string(error));
    return error >= 0;
}

bool MpvObject::set(const QString &name, const QVariant &value) {
    const QByteArray text = toArgument(value);
    const int error = mpv_set_property_string(m_mpv, name.toUtf8().constData(), text.constData());
    if (error < 0)
        qWarning("reel: setting %s to %s failed: %s", qPrintable(name), text.constData(), mpv_error_string(error));
    return error >= 0;
}

QVariant MpvObject::get(const QString &name) const {
    mpv_node node;
    if (mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &node) < 0)
        return {};
    const QVariant result = fromNode(&node);
    mpv_free_node_contents(&node);
    return result;
}

void MpvObject::togglePause() {
    // At the end of a file, play means watch it again.
    if (eof() && paused()) {
        runCommand(m_mpv, {"seek", "0", "absolute"});
        mpv_set_property_string(m_mpv, "pause", "no");
        return;
    }
    runCommand(m_mpv, {"cycle", "pause"});
}

void MpvObject::seek(double seconds, bool exact) {
    runCommand(m_mpv, {"seek", QByteArray::number(qMax(0.0, seconds), 'f', 3), exact ? "absolute+exact" : "absolute+keyframes"});
}

void MpvObject::seekRelative(double seconds) {
    runCommand(m_mpv, {"seek", QByteArray::number(seconds, 'f', 3), "relative+exact"});
}

void MpvObject::stop() {
    runCommand(m_mpv, {"stop"});
}

void MpvObject::playIndex(int index) {
    runCommand(m_mpv, {"playlist-play-index", QByteArray::number(index)});
    mpv_set_property_string(m_mpv, "pause", "no");
}

void MpvObject::screenshotTo(const QString &file, const QString &tag, bool withSubtitles) {
    const QList<QByteArray> args{"screenshot-to-file", file.toUtf8(), withSubtitles ? "subtitles" : "video"};
    const quint64 reply = m_nextReply++;
    m_screenshots.insert(reply, tag);
    runCommand(m_mpv, args, reply);
}
