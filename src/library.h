#pragma once

#include <QHash>
#include <QObject>
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

    // The playlist an open request becomes: a folder plays everything in it,
    // and a single file plays with its siblings of the same kind so the next
    // episode is ready. Returns {files, index}.
    Q_INVOKABLE QVariantMap expand(const QStringList &inputs) const;
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE QString frameCaptureFile(const QString &title, double position) const;
    Q_INVOKABLE QString formatTime(double seconds) const { return format(seconds); }
    Q_INVOKABLE QString relativeTime(qint64 msecsSinceEpoch) const;
    Q_INVOKABLE QString languageName(const QString &code) const;

    static QString format(double seconds);
    static bool isFinished(double position, double duration);
    static QString kindOf(const QString &path); // "video", "audio", or ""
    static QStringList mediaIn(const QString &directory);
    static void naturalSort(QStringList &paths);

signals:
    void changed();

private:
    QVariantMap toVariant(const Entry &entry) const;
    void load();
    void touch();

    QString m_dataFile;
    QString m_cacheDirectory;
    QHash<QString, Entry> m_entries;
    QTimer m_saveTimer;
    bool m_dirty = false;
};
