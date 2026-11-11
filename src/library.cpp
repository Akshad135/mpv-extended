#include "library.h"

#include <QClipboard>
#include <QCollator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

namespace {

const QStringList videoExtensions{"mkv", "mp4", "m4v", "webm", "avi", "mov", "wmv", "flv", "ts", "m2ts",
                                  "mts", "mpg", "mpeg", "ogv", "3gp", "vob", "divx", "rmvb", "asf"};
const QStringList audioExtensions{"mp3", "flac", "ogg", "oga", "opus", "m4a", "aac", "wav", "wv", "ape",
                                  "alac", "aiff", "wma", "mka"};

// Enough history to be useful without the file growing forever.
constexpr int historyLimit = 300;
// Long enough to outlive any other window that still holds a removed entry.
constexpr qint64 tombstoneLifetime = 30LL * 24 * 3600 * 1000;

QString localPath(const QString &input) {
    const QUrl url(input);
    if (url.isLocalFile())
        return url.toLocalFile();
    if (url.scheme().size() > 1) // http, ytdl, and anything else mpv opens itself
        return input;
    return QFileInfo(input).absoluteFilePath();
}

} // namespace

Library::Library(const QString &dataDirectory, const QString &cacheDirectory, QObject *parent)
    : QObject(parent) {
    const QString data = dataDirectory.isEmpty()
                             ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/mpv-extended"
                             : dataDirectory;
    m_cacheDirectory = cacheDirectory.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/mpv-extended"
                           : cacheDirectory;
    m_dataFile = data + "/history.json";
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(4000);
    connect(&m_saveTimer, &QTimer::timeout, this, &Library::save);
    load();
}

Library::~Library() {
    save();
}

// Reads a history file. *ok is false only when a file exists but can't be
// read or parsed; a missing file is simply an empty history.
QHash<QString, Library::Entry> Library::readEntries(const QString &path, Tombstones *forgotten, bool *ok) {
    QHash<QString, Entry> entries;
    forgotten->clear();
    *ok = true;
    QFile file(path);
    if (!file.exists())
        return entries;
    if (!file.open(QIODevice::ReadOnly)) {
        *ok = false;
        return entries;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        *ok = false;
        return entries;
    }
    const QJsonArray items = document.object().value("items").toArray();
    for (const auto &value : items) {
        const QJsonObject o = value.toObject();
        Entry e;
        e.path = o.value("path").toString();
        if (e.path.isEmpty())
            continue;
        e.title = o.value("title").toString();
        e.position = o.value("position").toDouble();
        e.duration = o.value("duration").toDouble();
        e.lastPlayed = qint64(o.value("lastPlayed").toDouble());
        e.thumbnailStamp = qint64(o.value("thumbnailStamp").toDouble());
        e.finished = o.value("finished").toBool();
        entries.insert(e.path, e);
    }
    const QJsonObject removed = document.object().value("forgotten").toObject();
    for (auto it = removed.begin(); it != removed.end(); ++it)
        forgotten->insert(it.key(), qint64(it.value().toDouble()));
    return entries;
}

void Library::load() {
    bool ok = true;
    m_entries = readEntries(m_dataFile, &m_forgotten, &ok);
    if (!ok)
        keepAside();
}

// A damaged history is moved aside rather than overwritten by the next save,
// so it can still be recovered by hand.
void Library::keepAside() {
    const QString aside = m_dataFile + ".bad-" + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (QFile::rename(m_dataFile, aside))
        qWarning("mpv-extended: history was unreadable; moved it to %s", qPrintable(aside));
    else
        qWarning("mpv-extended: history is unreadable and couldn't be moved aside: %s", qPrintable(m_dataFile));
}

void Library::save() {
    m_saveTimer.stop();
    if (!m_dirty)
        return;

    // One window at a time reads, merges, and writes, so neither drops the
    // other's save. A lock left by a crashed window goes stale and is taken.
    QDir().mkpath(QFileInfo(m_dataFile).absolutePath());
    QLockFile lock(m_dataFile + ".lock");
    lock.setStaleLockTime(10000);
    if (!lock.tryLock(2000)) {
        qWarning("mpv-extended: history is locked by another window; saving later");
        touch();
        return;
    }

    // Another window may have saved since this one loaded. Merge per
    // file, newest play wins, so neither window erases the other's progress.
    // A removal wins over any play from before it, whichever window saves
    // first.
    bool ok = true;
    Tombstones forgottenOnDisk;
    const auto onDisk = readEntries(m_dataFile, &forgottenOnDisk, &ok);
    if (!ok)
        keepAside(); // Damaged since this window loaded it.
    if (ok) {
        for (auto it = forgottenOnDisk.begin(); it != forgottenOnDisk.end(); ++it)
            if (it.value() > m_forgotten.value(it.key(), 0))
                m_forgotten.insert(it.key(), it.value());
        for (auto it = onDisk.begin(); it != onDisk.end(); ++it) {
            const auto mine = m_entries.constFind(it.key());
            if (mine == m_entries.constEnd() || mine->lastPlayed < it->lastPlayed)
                m_entries.insert(it.key(), it.value());
            // A removal made here outranks the copy it was meant to remove,
            // even one stamped ahead by a clock that has since stepped back.
            if (m_freshlyForgotten.contains(it.key()) && it->lastPlayed >= m_forgotten.value(it.key(), 0))
                m_forgotten.insert(it.key(), it->lastPlayed + 1);
        }
    }
    const qint64 expired = QDateTime::currentMSecsSinceEpoch() - tombstoneLifetime;
    bool removed = false;
    for (auto it = m_forgotten.begin(); it != m_forgotten.end();) {
        const auto entry = m_entries.find(it.key());
        if (entry != m_entries.end() && entry->lastPlayed <= it.value()) {
            QFile::remove(thumbnailFile(entry->path));
            m_entries.erase(entry);
            removed = true;
        }
        if (it.value() < expired)
            it = m_forgotten.erase(it);
        else
            ++it;
    }
    trim();

    QJsonArray items;
    for (const auto &e : std::as_const(m_entries))
        items.append(QJsonObject{{"path", e.path}, {"title", e.title}, {"position", e.position},
                                 {"duration", e.duration}, {"lastPlayed", double(e.lastPlayed)},
                                 {"thumbnailStamp", double(e.thumbnailStamp)}, {"finished", e.finished}});
    QJsonObject forgotten;
    for (auto it = m_forgotten.cbegin(); it != m_forgotten.cend(); ++it)
        forgotten.insert(it.key(), double(it.value()));
    QDir().mkpath(QFileInfo(m_dataFile).absolutePath());
    // Written beside the target and renamed over it, so a crash mid-write
    // never leaves a half history behind.
    QSaveFile file(m_dataFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("mpv-extended: can't write %s", qPrintable(m_dataFile));
        return;
    }
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"items", items}, {"forgotten", forgotten}})
                   .toJson(QJsonDocument::Compact));
    if (file.commit()) {
        m_dirty = false;
        m_freshlyForgotten.clear();
    }
    else
        qWarning("mpv-extended: can't save %s", qPrintable(m_dataFile));
    if (removed)
        emit changed();
}

void Library::touch() {
    m_dirty = true;
    if (!m_saveTimer.isActive())
        m_saveTimer.start();
}

void Library::refresh() {
    emit changed();
}

void Library::trim() {
    while (m_entries.size() > historyLimit) {
        auto oldest = std::min_element(m_entries.begin(), m_entries.end(),
                                       [](const Entry &a, const Entry &b) { return a.lastPlayed < b.lastPlayed; });
        QFile::remove(thumbnailFile(oldest->path));
        m_entries.erase(oldest);
    }
}

bool Library::isFinished(double position, double duration) {
    if (duration <= 0)
        return false;
    // The credits count as the end: the last 5%, at least 10 seconds and at
    // most a minute and a half.
    return duration - position <= std::clamp(duration * .05, 10.0, 90.0);
}

void Library::update(const QString &path, const QString &title, double position, double duration) {
    if (path.isEmpty())
        return;
    Entry &e = m_entries[path];
    e.path = path;
    if (!title.isEmpty())
        e.title = title;
    if (duration > 0)
        e.duration = duration;
    e.position = std::max(0.0, position);
    e.finished = isFinished(e.position, e.duration);
    // Always newer than what's there and than a removal of the same file,
    // even within one millisecond or with the clock stepped back, so the
    // newest play really wins the merge in save().
    e.lastPlayed = std::max({QDateTime::currentMSecsSinceEpoch(), e.lastPlayed + 1, m_forgotten.value(path, 0) + 1});
    trim();
    // Saved, but the lists aren't rebuilt: the library isn't on screen while
    // something plays, and refresh() updates it on the way back.
    touch();
}

void Library::forget(const QString &path) {
    // After the entry's own stamp, which may be ahead of the clock.
    const auto entry = m_entries.constFind(path);
    const qint64 played = entry == m_entries.constEnd() ? 0 : entry->lastPlayed;
    m_forgotten.insert(path, std::max(QDateTime::currentMSecsSinceEpoch(), played + 1));
    m_freshlyForgotten.insert(path);
    touch();
    if (m_entries.remove(path)) {
        QFile::remove(thumbnailFile(path));
        emit changed();
    }
}

double Library::resumePosition(const QString &path) const {
    const auto it = m_entries.constFind(path);
    if (it == m_entries.constEnd() || it->finished || it->position < 15)
        return 0;
    // Short clips start over; only something worth coming back to resumes.
    if (it->duration > 0 && it->duration < 120)
        return 0;
    return it->position;
}

QString Library::thumbnailFile(const QString &path) const {
    const QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex();
    QDir().mkpath(m_cacheDirectory + "/covers");
    return m_cacheDirectory + "/covers/" + QString::fromLatin1(hash.left(20)) + ".jpg";
}

void Library::thumbnailUpdated(const QString &path) {
    auto it = m_entries.find(path);
    if (it == m_entries.end())
        return;
    it->thumbnailStamp = QDateTime::currentMSecsSinceEpoch();
    touch();
}

QVariantMap Library::toVariant(const Entry &e) const {
    const QFileInfo info(e.path);
    const bool local = info.isAbsolute() && !e.path.contains("://");
    const QString thumb = thumbnailFile(e.path);
    const bool hasThumb = e.thumbnailStamp > 0 && QFileInfo::exists(thumb);
    QString title = e.title;
    if (title.isEmpty() || (local && title == info.fileName()))
        title = local ? info.completeBaseName() : e.path;
    return {
        {"path", e.path},
        {"title", title},
        {"folder", local ? info.dir().dirName() : QUrl(e.path).host()},
        {"position", e.position},
        {"duration", e.duration},
        {"progress", e.duration > 0 ? std::clamp(e.position / e.duration, 0.0, 1.0) : 0.0},
        {"remaining", std::max(0.0, e.duration - e.position)},
        {"finished", e.finished},
        {"lastPlayed", e.lastPlayed},
        {"available", !local || info.exists()},
        // The stamp in the query makes the image reload after a new frame.
        {"thumbnail", hasThumb ? QUrl::fromLocalFile(thumb).toString() + "?v=" + QString::number(e.thumbnailStamp)
                               : QString()},
        {"kind", local ? kindOf(e.path) : QString("video")},
    };
}

QVariantList Library::continueWatching() const {
    QList<Entry> list;
    for (const auto &e : m_entries)
        if (resumePosition(e.path) > 0)
            list.append(e);
    std::sort(list.begin(), list.end(), [](const Entry &a, const Entry &b) { return a.lastPlayed > b.lastPlayed; });
    QVariantList out;
    for (const auto &e : list.mid(0, 12))
        out.append(toVariant(e));
    return out;
}

QVariantList Library::recent() const {
    QList<Entry> list;
    for (const auto &e : m_entries)
        if (resumePosition(e.path) <= 0)
            list.append(e);
    std::sort(list.begin(), list.end(), [](const Entry &a, const Entry &b) { return a.lastPlayed > b.lastPlayed; });
    QVariantList out;
    for (const auto &e : list.mid(0, 40))
        out.append(toVariant(e));
    return out;
}

QString Library::kindOf(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (videoExtensions.contains(suffix))
        return "video";
    if (audioExtensions.contains(suffix))
        return "audio";
    return {};
}

void Library::naturalSort(QStringList &paths) {
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(paths.begin(), paths.end(), [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
}

QStringList Library::mediaIn(const QString &directory) {
    QStringList files;
    QDirIterator it(directory, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    // Bounded by everything visited, not just media found, so opening a huge
    // tree (a home folder, a source checkout) can't stall the window for long.
    int visited = 0;
    while (it.hasNext() && files.size() < 5000 && ++visited <= 50000) {
        const QString file = it.next();
        if (!kindOf(file).isEmpty())
            files.append(file);
    }
    if (visited > 50000)
        qWarning("mpv-extended: stopped scanning %s after 50000 files", qPrintable(directory));
    naturalSort(files);
    return files;
}

QVariantMap Library::expand(const QStringList &inputs) const {
    QStringList files;
    int index = 0;
    if (inputs.size() == 1) {
        const QString path = localPath(inputs.first());
        const QFileInfo info(path);
        if (info.isDir()) {
            files = mediaIn(path);
        } else if (info.isFile()) {
            const QString kind = kindOf(path);
            if (!kind.isEmpty()) {
                // A hidden file brings its hidden siblings; a normal one doesn't.
                const QDir::Filters filter = QDir::Files | QDir::Readable
                                             | (info.fileName().startsWith('.') ? QDir::Hidden : QDir::Filters());
                for (const auto &sibling : info.dir().entryInfoList(filter))
                    if (kindOf(sibling.fileName()) == kind)
                        files.append(sibling.absoluteFilePath());
                naturalSort(files);
            }
            index = files.indexOf(info.absoluteFilePath());
            if (index < 0) {
                files = {info.absoluteFilePath()};
                index = 0;
            }
        } else if (!path.isEmpty()) {
            files = {path};
        }
    } else {
        for (const auto &input : inputs) {
            const QString path = localPath(input);
            if (QFileInfo(path).isDir())
                files += mediaIn(path);
            else if (!path.isEmpty())
                files.append(path);
        }
    }
    return {{"files", files}, {"index", index}};
}

QString Library::clipboardText() const {
    return QGuiApplication::clipboard()->text().trimmed();
}

QString Library::frameCaptureFile(const QString &title, double position) const {
    QString directory = m_captureDirectory;
    if (directory.isEmpty()) {
        QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (pictures.isEmpty())
            pictures = QDir::homePath();
        directory = pictures + "/mpv-extended";
    }
    QDir().mkpath(directory);
    QString name = title;
    name.replace(QRegularExpression(R"([/\\:*?"<>|]+)"), " ");
    name = name.simplified().left(80);
    if (name.isEmpty())
        name = "frame";
    const QString stamp = format(position).replace(':', '.');
    // Names handed out are remembered too: the picture is written later, so
    // two captures within the same second would otherwise share one name.
    QString file = QString("%1/%2 %3.jpg").arg(directory, name, stamp);
    for (int n = 2; QFileInfo::exists(file) || m_capturesGiven.contains(file); ++n)
        file = QString("%1/%2 %3 (%4).jpg").arg(directory, name, stamp, QString::number(n));
    m_capturesGiven.insert(file);
    return file;
}

QString Library::format(double seconds) {
    if (!(seconds >= 0))
        seconds = 0;
    const qint64 total = qint64(seconds);
    const qint64 h = total / 3600, m = (total / 60) % 60, s = total % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

QString Library::relativeTime(qint64 msecsSinceEpoch) const {
    const qint64 minutes = (QDateTime::currentMSecsSinceEpoch() - msecsSinceEpoch) / 60000;
    if (minutes < 1)
        return tr("Just now");
    if (minutes < 60)
        return tr("%n min ago", "", int(minutes));
    if (minutes < 60 * 24)
        return tr("%n h ago", "", int(minutes / 60));
    if (minutes < 60 * 24 * 7)
        return tr("%n d ago", "", int(minutes / (60 * 24)));
    return QLocale().toString(QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch).date(), QLocale::ShortFormat);
}

QString Library::languageName(const QString &code) const {
    // Undetermined, no language, and miscellaneous say nothing useful.
    static const QStringList unknown{"und", "zxx", "mis", "mul", "unk", "none"};
    if (code.isEmpty() || unknown.contains(code.toLower()))
        return {};
    // Region tags (pt-BR, en_US) name the language they start with.
    const QString base = code.section(QRegularExpression("[-_]"), 0, 0);
    const QLocale::Language language = QLocale::codeToLanguage(base, QLocale::AnyLanguageCode);
    if (language == QLocale::AnyLanguage || language == QLocale::C)
        return code;
    // Qt's plain name ("German"). Native names were tried, but Qt picks a
    // likely region for them ("American English"), which reads wrong here.
    return QLocale::languageToString(language);
}
