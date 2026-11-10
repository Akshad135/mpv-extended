#pragma once

#include <QHash>
#include <QQuickFramebufferObject>
#include <QVariant>
#include <functional>

#include <mpv/client.h>
#include <mpv/render_gl.h>

// libmpv drawn into a Qt Quick item through mpv's OpenGL render API. mpv does
// the decoding, subtitles, and audio; this class turns its observed properties
// into Qt properties and its commands into invokable methods for the QML.
class MpvObject : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double cacheEnd READ cacheEnd NOTIFY cacheEndChanged)
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int chapter READ chapter NOTIFY chapterChanged)
    Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)
    Q_PROPERTY(int playlistPos READ playlistPos NOTIFY playlistPosChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    Q_PROPERTY(bool eof READ eof NOTIFY eofChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(bool subtitlesVisible READ subtitlesVisible NOTIFY subtitlesVisibleChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged)

    friend class MpvRenderer;

public:
    explicit MpvObject(QQuickItem *parent = nullptr);
    ~MpvObject() override;
    Renderer *createRenderer() const override;

    // Where a file should start, asked just before mpv opens it, so resuming
    // works the same for the first file and for every playlist step after it.
    static void setResumeLookup(std::function<double(const QString &)> lookup);

    bool paused() const { return value("pause").toBool(); }
    double position() const { return value("time-pos").toDouble(); }
    double duration() const { return value("duration").toDouble(); }
    double cacheEnd() const { return value("demuxer-cache-time").toDouble(); }
    double volume() const { return value("volume").toDouble(); }
    bool muted() const { return value("mute").toBool(); }
    double speed() const { return value("speed").toDouble(); }
    QString title() const { return value("media-title").toString(); }
    QString path() const { return value("path").toString(); }
    QVariantList tracks() const { return value("track-list").toList(); }
    QVariantList chapters() const { return value("chapter-list").toList(); }
    int chapter() const { return value("chapter").isValid() ? value("chapter").toInt() : -1; }
    QVariantList playlist() const { return value("playlist").toList(); }
    int playlistPos() const { return value("playlist-pos").isValid() ? value("playlist-pos").toInt() : -1; }
    bool idle() const { return !value("idle-active").isValid() || value("idle-active").toBool(); }
    bool eof() const { return value("eof-reached").toBool(); }
    bool buffering() const { return value("paused-for-cache").toBool(); }
    bool subtitlesVisible() const { return !value("sub-visibility").isValid() || value("sub-visibility").toBool(); }
    QVariantMap metadata() const { return value("metadata").toMap(); }

    Q_INVOKABLE void loadFiles(const QStringList &files, int index = 0);
    Q_INVOKABLE void command(const QVariantList &args);
    Q_INVOKABLE void set(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant get(const QString &name) const;
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(double seconds, bool exact = true);
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void playIndex(int index);
    // Writes the current frame to file in the background; screenshotSaved(tag)
    // reports a finished write. There is deliberately no waiting version:
    // blocking the GUI thread on mpv can deadlock with Qt's render thread.
    Q_INVOKABLE void screenshotTo(const QString &file, const QString &tag = {},
                                  bool withSubtitles = false);

signals:
    void pausedChanged();
    void positionChanged();
    void durationChanged();
    void cacheEndChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void titleChanged();
    void pathChanged();
    void tracksChanged();
    void chaptersChanged();
    void chapterChanged();
    void playlistChanged();
    void playlistPosChanged();
    void idleChanged();
    void eofChanged();
    void bufferingChanged();
    void subtitlesVisibleChanged();
    void metadataChanged();
    void fileLoaded();
    void resumed(double position);
    void failed(const QString &path, const QString &reason);
    void screenshotSaved(const QString &tag);

private:
    QVariant value(const char *name) const { return m_values.value(QString::fromLatin1(name)); }
    void processEvents();
    void renderReady();
    void handleEvent(mpv_event *event);

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_render = nullptr;
    QHash<QString, QVariant> m_values;
    QHash<quint64, QString> m_screenshots;
    quint64 m_nextReply = 1;
    QString m_loadingPath;
    bool m_ready = false;
    QStringList m_pendingFiles;
    int m_pendingIndex = 0;
};
