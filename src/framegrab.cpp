#include "framegrab.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <cstdio>

void grabFrameWithFfmpeg(QObject *owner, const QString &input, double position, const QString &output,
                         std::function<void(bool)> done, const QString &ffmpeg) {
    const QString program = ffmpeg.isEmpty() ? QStandardPaths::findExecutable("ffmpeg") : ffmpeg;
    if (program.isEmpty() || !QFileInfo(input).isFile() || output.isEmpty()) {
        done(false);
        return;
    }
    // Written beside the target and renamed into place, so nothing ever
    // sees half a picture.
    // Unique per process and per grab: two grabs of one file can overlap.
    static int serial = 0;
    const QString partial =
        QString("%1.%2-%3.part.jpg").arg(output).arg(QCoreApplication::applicationPid()).arg(++serial);
    auto *process = new QProcess(owner);
    // QProcess waits for ffmpeg to exit before destroyed() fires, so a killed
    // run, or the app quitting mid-frame, leaves nothing behind.
    QObject::connect(process, &QObject::destroyed, [partial] { QFile::remove(partial); });
    QObject::connect(process, &QProcess::finished, owner, [process, partial, output, done](int code) {
        process->deleteLater();
        const bool ok = code == 0 && QFileInfo(partial).size() > 0 &&
                        ::rename(QFile::encodeName(partial).constData(), QFile::encodeName(output).constData()) == 0;
        done(ok);
    });
    QObject::connect(process, &QProcess::errorOccurred, owner, [process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        done(false);
    });
    process->start(program, {"-hide_banner", "-loglevel", "error", "-nostdin", "-ss",
                             QString::number(qMax(0.0, position), 'f', 3), "-i", input, "-frames:v", "1",
                             "-an", "-sn", "-q:v", "3",
                             // One picture, not a numbered sequence: a "%d" in the
                             // title must stay literal.
                             "-update", "1", "-y", partial});
}
