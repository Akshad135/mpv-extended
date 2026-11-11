#pragma once

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <functional>

class Library;

// Playlists kept in the library: folders and playlist files that were
// opened, and queues saved by name. Each one remembers its files; a folder is
// scanned again whenever it's opened, so new episodes show up. Where to pick
// up comes from the watch history.
//
// Kept in their own JSON file. Every change is applied to what's on disk,
// under a lock, so two windows never undo each other's changes.
class Playlists : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList all READ all NOTIFY changed)

public:
    struct Item {
        QString id;
        QString name;
        QString kind; // "folder", "file" (an .m3u or .pls), or "saved"
        QString source; // The folder or playlist file; empty when saved.
        QStringList files;
        qint64 lastOpened = 0;
    };

    explicit Playlists(const Library *library, const QString &dataDirectory = {}, QObject *parent = nullptr);

    QVariantList all() const;

    // Keeps an opened folder or playlist file ({kind, source, name}, from
    // Library::expand) with its files, and returns its id (empty if it
    // couldn't be saved).
    Q_INVOKABLE QString remember(const QVariantMap &collection, const QStringList &files);
    // Keeps a list under a name (a saved queue, or a new empty playlist),
    // and returns its id (empty if it couldn't be saved).
    Q_INVOKABLE QString save(const QString &name, const QStringList &files = {});
    Q_INVOKABLE void forget(const QString &id);
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    // Replaces the files (added, removed, reordered). A folder or playlist
    // file that's edited becomes a list of its own: it stops following the
    // folder, and gets a new id, which is returned (empty on failure).
    Q_INVOKABLE QString setFiles(const QString &id, const QStringList &files);
    // One playlist as the library shows it, or empty if there's none.
    Q_INVOKABLE QVariantMap find(const QString &id) const;
    // Its files, each with what the history knows: {path, title, folder,
    // duration, finished, available}.
    Q_INVOKABLE QVariantList entries(const QString &id) const;
    // What to play: {files, index}, starting at the given file, else where
    // the history says to. A folder is scanned again first. Empty files if
    // it's gone or empty.
    Q_INVOKABLE QVariantMap open(const QString &id, const QString &startAt = {});
    // Where to pick these files up, from the watch history.
    Q_INVOKABLE int startAt(const QStringList &files) const { return startIndex(files, m_library); }
    // Reads the file again, for changes made in another window.
    Q_INVOKABLE void refresh();

    // Where to pick a list up: after the last finished file, at the last
    // unfinished one, or at the start.
    static int startIndex(const QStringList &files, const Library *library);

signals:
    void changed();

private:
    QList<Item> read(bool *ok) const;
    bool change(const std::function<void(QList<Item> &)> &edit);
    QVariantMap toVariant(const Item &item) const;

    const Library *m_library;
    QString m_dataFile;
    QList<Item> m_items;
};
