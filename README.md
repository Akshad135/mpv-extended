# Reel

A video player for Omarchy. libmpv plays everything mpv plays; Reel is the
part you see: a library that remembers where you stopped, a quiet player that
gets out of the way, and colors and corners that follow your Omarchy theme
live.

## What it does

- **Continue watching.** Progress is saved as you watch. Opening a file again
  resumes it; the library shows the frame where you stopped.
- **Queues itself.** Open one episode and the rest of the folder is queued in
  natural order (2 before 10). At the end, an "Up next" card counts down.
- **Timeline previews.** Hover the seek bar to see the frame, the time, and
  the chapter.
- **Tracks.** Pick audio and subtitle tracks by language, or load a
  subtitle file.
- **Links.** Paste a link (Ctrl+V); yt-dlp handles the rest.
- **Keeps the screen awake** while video plays: Wayland idle-inhibit for
  Hyprland and Omarchy, and the desktop portal for GNOME and KDE.
- **Follows Omarchy.** The whole palette, light and dark themes, and
  Hyprland's corner rounding, live. Text follows the Omarchy language once a
  translation exists; only English ships today.
- **Works elsewhere too.** Off Omarchy it falls back to a neutral dark theme.

## Shortcuts

| Key | Action |
| --- | --- |
| Space, K | Play or pause |
| ← → | Back or forward 5 s (Shift: 1 s) |
| J L | Back or forward 10 s |
| , . | Previous or next frame |
| 0–9 | Jump to 0–90% |
| PgUp PgDn | Previous or next chapter |
| [ ] / Backspace | Slower or faster / normal speed |
| ↑ ↓ / M | Volume / mute |
| A / S / T | Next audio / next subtitles / track menu |
| N / Shift+N | Next or previous file |
| P | Queue |
| F, F11 | Fullscreen |
| Enter | At the end of a file: play the next one |
| Ctrl+S | Save the frame to ~/Pictures/Reel |
| Ctrl+O / Ctrl+Shift+O / Ctrl+V | Open files / a folder / a copied link |
| Esc | Close, leave fullscreen, or go to the library |
| ? | All shortcuts |
| Q | Quit |

## Build

Needs `base-devel qt6-base qt6-declarative mpv ffmpeg wayland-protocols`.
The file picker needs an xdg-desktop-portal backend that provides a file
chooser, such as `xdg-desktop-portal-gtk` (Omarchy has it).

```sh
bin/build            # build/reel (CONFIG+=dev adds a screenshot hook for testing)
bin/test             # unit tests, offscreen
bin/install          # makepkg -fsi: installs reel, its icon, and launcher entry
build/reel ~/Videos  # play a folder
```

History lives in `~/.local/share/reel/history.json`; frames are cached in
`~/.cache/reel`.
