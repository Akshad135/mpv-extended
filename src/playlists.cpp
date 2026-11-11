#include "playlists.h"

#include "library.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

namespace {
// Kept to a size that stays quick to read and show.
constexpr int playlistLimit = 100;
constexpr int filesPerPlaylist = 5000;
} // namespace

Playlists::Playlists(const Library *library, const QString &dataDirectory, QObject *parent)
    : QObject(parent), m_library(library) {
    const QString data = dataDirectory.isEmpty()
                             ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/mpv-extended"
                             : dataDirectory;
    m_dataFile = data + "/playlists.json";
    bool ok = true;
    m_items = read(&ok);
    // Progress shows from the history, so it follows every change there.
    if (library)
        connect(library, &Library::changed, this, &Playlists::changed);
}

QList<Playlists::Item> Playlists::read(bool *ok) const {
    QList<Item> items;
    *ok = true;
    QFile file(m_dataFile);
    if (!file.exists())
        return items;
    if (!file.open(QIODevice::ReadOnly)) {
        *ok = false;
        return items;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        *ok = false;
        return items;
    }
    for (const auto &value : document.object().value("playlists").toArray()) {
        const QJsonObject o = value.toObject();
        Item item;
        item.id = o.value("id").toString();
        if (item.id.isEmpty())
            continue;
        item.name = o.value("name").toString();
        item.kind = o.value("kind").toString();
        item.source = o.value("source").toString();
        for (const auto &f : o.value("files").toArray())
            item.files.append(f.toString());
        item.lastOpened = qint64(o.value("lastOpened").toDouble());
        items.append(item);
    }
    return items;
}

bool Playlists::change(const std::function<void(QList<Item> &)> &edit) {
    QDir().mkpath(QFileInfo(m_dataFile).absolutePath());
    QLockFile lock(m_dataFile + ".lock");
    lock.setStaleLockTime(10000);
    if (!lock.tryLock(2000)) {
        qWarning("mpv-extended: playlists are locked by another window");
        return false;
    }
    bool ok = true;
    QList<Item> items = read(&ok);
    if (!ok) {
        // Kept for recovery by hand rather than overwritten.
        const QString aside = m_dataFile + ".bad-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        if (!QFile::rename(m_dataFile, aside)) {
            qWarning("mpv-extended: playlists are unreadable and couldn't be moved aside: %s", qPrintable(m_dataFile));
            return false;
        }
        qWarning("mpv-extended: playlists were unreadable; moved them to %s", qPrintable(aside));
    }
    edit(items);
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.lastOpened > b.lastOpened; });
    // The least recently opened folders and files go first; a queue saved by
    // name is never dropped on its own.
    for (int i = items.size() - 1; items.size() > playlistLimit && i >= 0; --i)
        if (items[i].kind != "saved")
            items.removeAt(i);

    QJsonArray array;
    for (const auto &item : std::as_const(items))
        array.append(QJsonObject{{"id", item.id}, {"name", item.name}, {"kind", item.kind}, {"source", item.source},
                                 {"files", QJsonArray::fromStringList(item.files)},
                                 {"lastOpened", double(item.lastOpened)}});
    // Written beside the target and renamed over it: never half a file.
    QSaveFile file(m_dataFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("mpv-extended: can't write %s", qPrintable(m_dataFile));
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"playlists", array}}).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qWarning("mpv-extended: can't save %s", qPrintable(m_dataFile));
        return false;
    }
    m_items = items;
    emit changed();
    return true;
}

QString Playlists::remember(const QVariantMap &collection, const QStringList &files) {
    const QString kind = collection.value("kind").toString();
    const QString source = collection.value("source").toString();
    if (source.isEmpty() || (kind != "folder" && kind != "file"))
        return {};
    // One playlist per folder or file, however often it's opened.
    const QString id = kind + ":" + source;
    const QString name = collection.value("name").toString();
    const bool ok = change([&](QList<Item> &items) {
        auto it = std::find_if(items.begin(), items.end(), [&](const Item &i) { return i.id == id; });
        if (it == items.end())
            it = items.insert(items.end(), Item{id, name, kind, source, {}, 0});
        it->name = name.isEmpty() ? QFileInfo(source).fileName() : name;
        it->files = files.mid(0, filesPerPlaylist);
        it->lastOpened = QDateTime::currentMSecsSinceEpoch();
    });
    return ok ? id : QString();
}

QString Playlists::save(const QString &name, const QStringList &files) {
    const QString id = QString("saved:%1-%2")
                           .arg(QDateTime::currentMSecsSinceEpoch())
                           .arg(QRandomGenerator::global()->generate(), 8, 16, QChar('0'));
    const QString trimmed = name.simplified();
    const bool ok = change([&](QList<Item> &items) {
        items.append(Item{id, trimmed.isEmpty() ? tr("Playlist") : trimmed, "saved", {},
                          files.mid(0, filesPerPlaylist), QDateTime::currentMSecsSinceEpoch()});
    });
    return ok ? id : QString();
}

void Playlists::rename(const QString &id, const QString &name) {
    const QString trimmed = name.simplified();
    if (trimmed.isEmpty())
        return;
    change([&](QList<Item> &items) {
        for (auto &item : items)
            if (item.id == id)
                item.name = trimmed;
    });
}

QString Playlists::setFiles(const QString &id, const QStringList &files) {
    QString result;
    const bool ok = change([&](QList<Item> &items) {
        for (auto &item : items) {
            if (item.id != id)
                continue;
            if (item.kind != "saved") {
                // Its own list from now on: opening the folder again makes
                // a fresh folder playlist instead of undoing these edits.
                item.kind = "saved";
                item.source.clear();
                item.id = QString("saved:%1-%2")
                              .arg(QDateTime::currentMSecsSinceEpoch())
                              .arg(QRandomGenerator::global()->generate(), 8, 16, QChar('0'));
            }
            item.files = files.mid(0, filesPerPlaylist);
            result = item.id;
        }
    });
    return ok ? result : QString();
}

QVariantMap Playlists::find(const QString &id) const {
    for (const auto &item : m_items)
        if (item.id == id)
            return toVariant(item);
    return {};
}

QVariantList Playlists::entries(const QString &id) const {
    QVariantList out;
    for (const auto &item : m_items) {
        if (item.id != id)
            continue;
        Library::Entry entry;
        for (const auto &file : item.files) {
            const bool link = file.contains("://");
            const QFileInfo info(file);
            const bool played = m_library && m_library->lookup(file, &entry);
            out.append(QVariantMap{
                {"path", file},
                {"title", link ? (played && !entry.title.isEmpty() ? entry.title : file) : info.completeBaseName()},
                {"folder", link ? QUrl(file).host() : info.dir().dirName()},
                {"duration", played ? entry.duration : 0.0},
                {"finished", played && entry.finished},
                {"available", link || info.exists()},
            });
        }
    }
    return out;
}

void Playlists::forget(const QString &id) {
    change([&](QList<Item> &items) {
        items.removeIf([&](const Item &i) { return i.id == id; });
    });
}

void Playlists::refresh() {
    bool ok = true;
    const QList<Item> items = read(&ok);
    if (ok)
        m_items = items;
    emit changed();
}

QVariantMap Playlists::open(const QString &id, const QString &startAt, int near) {
    const QVariantMap none{{"files", QStringList()}, {"index", 0}};
    auto it = std::find_if(m_items.cbegin(), m_items.cend(), [&](const Item &i) { return i.id == id; });
    if (it == m_items.cend()) {
        // Perhaps made in another window since this one last read.
        refresh();
        it = std::find_if(m_items.cbegin(), m_items.cend(), [&](const Item &i) { return i.id == id; });
    }
    if (it == m_items.cend() || (it->kind == "folder" && !QFileInfo(it->source).isDir()))
        return none;
    // The files come from what's on disk, read under the lock: another
    // window's edits since this one last read must survive the open.
    QStringList files;
    bool found = false;
    change([&](QList<Item> &items) {
        for (auto &item : items) {
            if (item.id != id)
                continue;
            found = true;
            if (item.kind == "folder" && QFileInfo(item.source).isDir())
                item.files = Library::mediaIn(item.source).mid(0, filesPerPlaylist);
            else if (item.kind == "file" && QFileInfo(item.source).isFile())
                item.files = Library::readPlaylistFile(item.source).mid(0, filesPerPlaylist);
            item.lastOpened = QDateTime::currentMSecsSinceEpoch();
            files = item.files;
        }
    });
    if (!found)
        return none;
    // A file listed twice plays from the copy that was picked.
    int chosen = near >= 0 && near < files.size() && files[near] == startAt ? near : -1;
    if (chosen < 0 && !startAt.isEmpty())
        chosen = int(files.indexOf(startAt));
    return {{"files", files}, {"index", chosen >= 0 ? chosen : startIndex(files, m_library)}};
}

int Playlists::startIndex(const QStringList &files, const Library *library) {
    if (!library)
        return 0;
    int last = -1;
    Library::Entry latest, entry;
    for (int i = 0; i < files.size(); ++i)
        if (library->lookup(files[i], &entry) && (last < 0 || entry.lastPlayed > latest.lastPlayed)) {
            last = i;
            latest = entry;
        }
    if (last < 0)
        return 0;
    if (!latest.finished)
        return last;
    // Finished: the next one, or from the top once the end was reached.
    return last + 1 < files.size() ? last + 1 : 0;
}

QVariantMap Playlists::toVariant(const Item &item) const {
    int watched = 0;
    Library::Entry entry;
    for (const auto &file : item.files)
        if (m_library && m_library->lookup(file, &entry) && entry.finished)
            ++watched;
    const int next = startIndex(item.files, m_library);
    const QString nextFile = item.files.value(next);
    // The frame of the file it picks up at, else of any file played.
    QString thumbnail = m_library ? m_library->thumbnailUrl(nextFile) : QString();
    for (int i = 0; thumbnail.isEmpty() && m_library && i < item.files.size(); ++i)
        thumbnail = m_library->thumbnailUrl(item.files[i]);
    const QFileInfo nextInfo(nextFile);
    const bool started = std::any_of(item.files.cbegin(), item.files.cend(), [&](const QString &f) {
        return m_library && m_library->lookup(f, &entry);
    });
    const bool available = item.kind == "folder" ? QFileInfo(item.source).isDir()
                         : item.kind == "file"   ? QFileInfo(item.source).isFile()
                                                 : true;
    return {
        {"id", item.id},
        {"name", item.name},
        {"kind", item.kind},
        {"count", item.files.size()},
        {"watched", watched},
        {"progress", item.files.isEmpty() ? 0.0 : double(watched) / item.files.size()},
        {"started", started},
        {"next", next},
        {"nextTitle", nextFile.contains("://") ? nextFile : nextInfo.completeBaseName()},
        {"thumbnail", thumbnail},
        {"available", available},
        {"lastOpened", item.lastOpened},
    };
}

QVariantList Playlists::all() const {
    QVariantList out;
    for (const auto &item : m_items)
        out.append(toVariant(item));
    return out;
}
