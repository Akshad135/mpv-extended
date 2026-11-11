#pragma once

#include <QString>
#include <QStringList>
#include <functional>

class QObject;

// Saves one frame of a local file with ffmpeg. Used when mpv can't take the
// screenshot itself: its software fallback can't read frames that stay on an
// NVIDIA GPU (nvdec), and the GPU path it has instead would tie mpv to Qt's
// render thread, which stops whenever the window is hidden. Subtitles aren't
// included. done(ok) runs on the owner's thread; never if the owner dies first.
//
// A quick grab is for small pictures (library cards): the nearest keyframe,
// scaled down, decoded on one thread at low priority. Otherwise the frame is
// exact and full size.
void grabFrameWithFfmpeg(QObject *owner, const QString &input, double position, const QString &output,
                         std::function<void(bool)> done, const QString &ffmpeg = {}, bool quick = false);

// Runs a helper process (ffmpeg) at low CPU and disk priority, so the video
// itself never waits on it.
void runInBackground(class QProcess *process);

// ffmpeg's input options for a cheap, inexact frame: one thread, landing on
// the keyframe before the position. Decoding only keyframes is far cheaper
// still, but can come back empty in a file's last stretch, so callers retry
// once without it.
QStringList cheapSeek(double position, bool keyframesOnly);

// The ffmpeg beside the app's binary (the AppImage bundles one), else the
// one on PATH. Empty if there's neither.
QString ffmpegProgram();
