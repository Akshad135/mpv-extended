#pragma once

#include <QList>
#include <QObject>
#include <QSet>

class QProcess;

// Frames for the timeline's hover preview. ffmpeg grabs them a few at a time
// from evenly spaced points, coarse to fine, so the preview is useful within
// a second and sharpens as the rest arrive. Frames are cached per file.
class Thumbnailer : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit Thumbnailer(const QString &cacheDirectory = {}, const QString &ffmpeg = {}, QObject *parent = nullptr);
    ~Thumbnailer() override;

    int revision() const { return m_revision; }
    Q_INVOKABLE void prepare(const QString &path, double duration);
    Q_INVOKABLE QString frameAt(double fraction) const;

    static int frameCount(double duration);
    static QList<int> order(int count);
    static int nearest(const QSet<int> &done, int wanted, int count);

signals:
    void revisionChanged();

private:
    void pump();
    void reset();
    void prune();

    QString m_cacheDirectory;
    QString m_ffmpeg;
    QString m_path;
    QString m_directory;
    double m_duration = 0;
    int m_count = 0;
    int m_revision = 0;
    QList<int> m_queue;
    QSet<int> m_done;
    QSet<int> m_slow; // Frames whose keyframe-only grab came back empty.
    QList<QProcess *> m_running;
};
