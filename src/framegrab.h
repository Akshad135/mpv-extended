#pragma once

#include <QString>
#include <functional>

class QObject;

// Saves one frame of a local file with ffmpeg. Used when mpv can't take the
// screenshot itself: its software fallback can't read frames that stay on an
// NVIDIA GPU (nvdec), and the GPU path it has instead would tie mpv to Qt's
// render thread, which stops whenever the window is hidden. Subtitles aren't
// included. done(ok) runs on the owner's thread; never if the owner dies first.
void grabFrameWithFfmpeg(QObject *owner, const QString &input, double position, const QString &output,
                         std::function<void(bool)> done, const QString &ffmpeg = {});
