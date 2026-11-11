#include "thumbnailer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utime.h>

namespace {
constexpr int parallel = 3;
}

Thumbnailer::Thumbnailer(const QString &cacheDirectory, const QString &ffmpeg, QObject *parent)
    : QObject(parent),
      m_cacheDirectory(cacheDirectory.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/mpv-extended/frames"
                           : cacheDirectory),
      m_ffmpeg(ffmpeg.isEmpty() ? QStandardPaths::findExecutable("ffmpeg") : ffmpeg) {
    prune();
}

// Frames of files not watched for a month are dropped, so the cache doesn't
// grow forever.
void Thumbnailer::prune() {
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-30);
    for (const QFileInfo &entry : QDir(m_cacheDirectory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (entry.lastModified() < cutoff)
            QDir(entry.absoluteFilePath()).removeRecursively();
}

Thumbnailer::~Thumbnailer() {
    reset();
}

int Thumbnailer::frameCount(double duration) {
    if (!(duration > 0))
        return 0;
    // One frame per 5 seconds, between 10 and 120 frames.
    return std::clamp(int(std::ceil(duration / 5)), 10, 120);
}

QList<int> Thumbnailer::order(int count) {
    QList<int> out;
    QSet<int> seen;
    for (int parts = 1; seen.size() < count && parts <= count * 2; parts *= 2)
        for (int j = 0; j < parts; ++j) {
            const int index = std::min(count - 1, int((j + .5) * count / parts));
            if (!seen.contains(index)) {
                seen.insert(index);
                out.append(index);
            }
        }
    for (int i = 0; i < count; ++i)
        if (!seen.contains(i))
            out.append(i);
    return out;
}

int Thumbnailer::nearest(const QSet<int> &done, int wanted, int count) {
    for (int d = 0; d < count; ++d) {
        if (done.contains(wanted - d))
            return wanted - d;
        if (done.contains(wanted + d))
            return wanted + d;
    }
    return -1;
}

void Thumbnailer::reset() {
    // Never waits: a killed ffmpeg cleans itself up when it exits.
    for (QProcess *process : std::as_const(m_running)) {
        process->disconnect(this);
        connect(process, &QProcess::finished, process, &QObject::deleteLater);
        connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
        process->kill();
    }
    m_running.clear();
    m_queue.clear();
    m_done.clear();
    m_count = 0;
}

void Thumbnailer::prepare(const QString &path, double duration) {
    const QFileInfo info(path);
    const int count = frameCount(duration);
    if (path == m_path && count == m_count && qAbs(duration - m_duration) < 1)
        return;
    reset();
    m_path = path;
    m_duration = duration;
    ++m_revision;
    emit revisionChanged();
    // Streams and missing tools get no preview; the timeline still works.
    if (m_ffmpeg.isEmpty() || !info.isFile() || count == 0)
        return;

    const QByteArray key = QCryptographicHash::hash(
        (info.absoluteFilePath() + "|" + QString::number(info.size()) + "|" +
         QString::number(info.lastModified().toMSecsSinceEpoch()) + "|" + QString::number(count) + "|" +
         QString::number(qRound64(duration))).toUtf8(),
        QCryptographicHash::Sha1).toHex();
    m_directory = m_cacheDirectory + "/" + QString::fromLatin1(key.left(20));
    QDir().mkpath(m_directory);
    // Touched so pruning sees it as in use.
    ::utime(QFile::encodeName(m_directory).constData(), nullptr);
    m_count = count;
    for (int index : order(count)) {
        if (QFileInfo::exists(QString("%1/%2.jpg").arg(m_directory).arg(index)))
            m_done.insert(index);
        else
            m_queue.append(index);
    }
    if (!m_done.isEmpty()) {
        ++m_revision;
        emit revisionChanged();
    }
    pump();
}

void Thumbnailer::pump() {
    while (m_running.size() < parallel && !m_queue.isEmpty()) {
        const int index = m_queue.takeFirst();
        const QString target = QString("%1/%2.jpg").arg(m_directory).arg(index);
        // Unique per process, so two windows previewing the same file
        // never write into each other's partial frame.
        const QString partial = QString("%1.%2.part.jpg").arg(target).arg(QCoreApplication::applicationPid());
        const double at = (index + .5) / m_count * m_duration;

        auto *process = new QProcess(this);
        // A frame that was killed or failed leaves no partial file behind.
        // QProcess waits for ffmpeg to exit before destroyed() fires, so this
        // also covers killed runs and the app quitting mid-frame.
        connect(process, &QObject::destroyed, [partial] { QFile::remove(partial); });
        process->setProgram(m_ffmpeg);
        process->setArguments({"-hide_banner", "-loglevel", "error", "-nostdin", "-ss", QString::number(at, 'f', 2),
                               "-i", m_path, "-frames:v", "1", "-an", "-sn", "-vf", "scale=320:-2", "-q:v", "5",
                               "-y", partial});
        connect(process, &QProcess::finished, this, [this, process, index, target, partial](int code) {
            m_running.removeOne(process);
            process->deleteLater();
            // Renamed into place only once complete, so a half-written frame
            // is never shown.
            if (code == 0 && QFileInfo(partial).size() > 0) {
                // rename(2) replaces the target atomically.
                if (::rename(QFile::encodeName(partial).constData(), QFile::encodeName(target).constData()) == 0) {
                    m_done.insert(index);
                    ++m_revision;
                    emit revisionChanged();
                }
            }
            pump();
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
                return;
            m_running.removeOne(process);
            process->deleteLater();
            m_queue.clear();
        });
        m_running.append(process);
        process->start();
    }
}

QString Thumbnailer::frameAt(double fraction) const {
    if (m_count == 0 || m_done.isEmpty())
        return {};
    const int wanted = std::clamp(int(fraction * m_count), 0, m_count - 1);
    const int index = nearest(m_done, wanted, m_count);
    if (index < 0)
        return {};
    return QUrl::fromLocalFile(QString("%1/%2.jpg").arg(m_directory).arg(index)).toString();
}
