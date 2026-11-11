#include "framegrab.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <cstdio>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

QString ffmpegProgram() {
    const QString bundled = QCoreApplication::instance()
                                ? QStandardPaths::findExecutable("ffmpeg", {QCoreApplication::applicationDirPath()})
                                : QString();
    return bundled.isEmpty() ? QStandardPaths::findExecutable("ffmpeg") : bundled;
}

void runInBackground(QProcess *process) {
    process->setChildProcessModifier([] {
        // Only async-signal-safe calls here: this runs between fork and exec.
        [[maybe_unused]] int ignored = ::setpriority(PRIO_PROCESS, 0, 15);
        // ioprio_set(IOPRIO_WHO_PROCESS, self, IOPRIO_CLASS_IDLE)
        ::syscall(SYS_ioprio_set, 1, 0, 3 << 13);
    });
}

QStringList cheapSeek(double position, bool keyframesOnly) {
    QStringList arguments{"-threads", "1", "-noaccurate_seek"};
    if (keyframesOnly)
        arguments << "-skip_frame" << "nokey";
    return arguments << "-ss" << QString::number(qMax(0.0, position), 'f', 3);
}

namespace {
enum class Grab { Exact, Keyframe, Nearby };

void grab(QObject *owner, const QString &program, const QString &input, double position, const QString &output,
          std::function<void(bool)> done, Grab how) {
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
    QObject::connect(process, &QProcess::finished, owner,
                     [owner, program, input, position, output, done, how, process, partial](int code) {
        process->deleteLater();
        const bool ok = code == 0 && QFileInfo(partial).size() > 0 &&
                        ::rename(QFile::encodeName(partial).constData(), QFile::encodeName(output).constData()) == 0;
        if (!ok && how == Grab::Keyframe)
            grab(owner, program, input, position, output, done, Grab::Nearby);
        else
            done(ok);
    });
    QObject::connect(process, &QProcess::errorOccurred, owner, [process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        done(false);
    });

    QStringList arguments{"-hide_banner", "-loglevel", "error", "-nostdin"};
    if (how == Grab::Exact)
        arguments << "-ss" << QString::number(qMax(0.0, position), 'f', 3);
    else
        arguments << cheapSeek(position, how == Grab::Keyframe);
    arguments << "-i" << input << "-frames:v" << "1" << "-an" << "-sn" << "-q:v" << "3";
    if (how != Grab::Exact) {
        arguments << "-vf" << "scale='min(640,iw)':-2";
        runInBackground(process);
    }
    // One picture, not a numbered sequence: a "%d" in the title must stay
    // literal.
    arguments << "-update" << "1" << "-y" << partial;
    process->start(program, arguments);
}
} // namespace

void grabFrameWithFfmpeg(QObject *owner, const QString &input, double position, const QString &output,
                         std::function<void(bool)> done, const QString &ffmpeg, bool quick) {
    const QString program = ffmpeg.isEmpty() ? ffmpegProgram() : ffmpeg;
    if (program.isEmpty() || !QFileInfo(input).isFile() || output.isEmpty()) {
        done(false);
        return;
    }
    grab(owner, program, input, position, output, std::move(done), quick ? Grab::Keyframe : Grab::Exact);
}
