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
    // Keeps a queue under a name, and returns its id (empty if it couldn't
    // be saved).
    Q_INVOKABLE QString save(const QString &name, const QStringList &files);
    Q_INVOKABLE void forget(const QString &id);
    // What to play: {files, index}, starting where the history says to. A
    // folder is scanned again first. Empty files if it's gone.
    Q_INVOKABLE QVariantMap open(const QString &id);
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
