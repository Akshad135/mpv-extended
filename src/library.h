#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariant>

// What the user has watched and where they stopped, kept in one JSON file,
// plus the file handling around it: which files a drop or an argument opens,
// and in what order.
class Library : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList continueWatching READ continueWatching NOTIFY changed)
    Q_PROPERTY(QVariantList recent READ recent NOTIFY changed)

public:
    struct Entry {
        QString path;
        QString title;
        double position = 0;
        double duration = 0;
        qint64 lastPlayed = 0;
        qint64 thumbnailStamp = 0;
        bool finished = false;
    };

    explicit Library(const QString &dataDirectory = {}, const QString &cacheDirectory = {}, QObject *parent = nullptr);
    ~Library() override;

    QVariantList continueWatching() const;
    QVariantList recent() const;

    Q_INVOKABLE void update(const QString &path, const QString &title, double position, double duration);
    Q_INVOKABLE void forget(const QString &path);
    Q_INVOKABLE double resumePosition(const QString &path) const;
    Q_INVOKABLE QString thumbnailFile(const QString &path) const;
    Q_INVOKABLE void thumbnailUpdated(const QString &path);
    Q_INVOKABLE void save();
    // Rebuilds the library lists; called when the library comes back on screen.
    Q_INVOKABLE void refresh();

    // The playlist an open request becomes: a folder plays everything in it,
    // a playlist file (.m3u, .m3u8, .pls) plays what it lists, and a single
    // file plays with its siblings of the same kind so the next episode is
    // ready. Returns {files, index}, plus {collection: {kind, source, name}}
    // when one folder or one playlist file was opened: those are kept as
    // playlists in the library.
    Q_INVOKABLE QVariantMap expand(const QStringList &inputs) const;
    // Files to add to a playlist: folders and playlist files are opened up,
    // and anything else is taken as it is (one file stays one file).
    Q_INVOKABLE QStringList collect(const QStringList &inputs) const;
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE QString frameCaptureFile(const QString &title, double position) const;
    // Where captures go; ~/Pictures/mpv-extended unless set (tests set it).
    void setCaptureDirectory(const QString &directory) { m_captureDirectory = directory; }
    Q_INVOKABLE QString formatTime(double seconds) const { return format(seconds); }
    Q_INVOKABLE QString relativeTime(qint64 msecsSinceEpoch) const;
    Q_INVOKABLE QString languageName(const QString &code) const;

    static QString format(double seconds);
    static bool isFinished(double position, double duration);
    // A finished-or-not record of one file, for playlists. False if unplayed.
    bool lookup(const QString &path, Entry *entry) const;
    // The library-card frame of a file, as a URL, or empty.
    QString thumbnailUrl(const QString &path) const;

    static QString kindOf(const QString &path); // "video", "audio", or ""
    static bool isPlaylistFile(const QString &path);
    // The entries of an .m3u, .m3u8, or .pls file, relative paths resolved.
    static QStringList readPlaylistFile(const QString &path);
    static QStringList mediaIn(const QString &directory);
    static void naturalSort(QStringList &paths);

signals:
    void changed();

private:
    // Removed entries, by path, with when they were removed. Saved with the
    // history so another window can't bring an entry back.
    using Tombstones = QHash<QString, qint64>;

    QVariantMap toVariant(const Entry &entry) const;
    static QHash<QString, Entry> readEntries(const QString &path, Tombstones *forgotten, bool *ok);
    void load();
    void keepAside();
    void touch();
    void trim();

    QString m_dataFile;
    QString m_cacheDirectory;
    QHash<QString, Entry> m_entries;
    Tombstones m_forgotten;
    mutable QSet<QString> m_capturesGiven;
    QSet<QString> m_freshlyForgotten; // Removed here since the last save.
    QString m_captureDirectory;
    QTimer m_saveTimer;
    bool m_dirty = false;
};
