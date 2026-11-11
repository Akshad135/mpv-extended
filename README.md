# mpv-extended

A video player for Omarchy. libmpv plays everything mpv plays; mpv-extended
is the part you see: a library that remembers where you stopped, a quiet
player that gets out of the way, and colors and corners that follow your
Omarchy theme live.

It is an independent project built on libmpv, not part of mpv.

## What it does

- **Continue watching.** Progress is saved as you watch. Opening a file again
  resumes it; the library shows the frame where you stopped.
- **Queues itself.** Open one episode and the rest of the folder is queued in
  natural order (2 before 10). At the end, an "Up next" card counts down.
- **Playlists.** Open a folder, or an .m3u, .m3u8, or .pls file, and it stays
  in the library as a playlist that picks up after the last file you
  watched. A folder is scanned again each time, so new episodes show up. Any
  queue can be saved as a playlist from the queue panel, and a new one made
  with Ctrl+N. In a playlist, rename it, add files or folders (or drop them
  in), drag files into order (or Ctrl+Up/Down), remove them, and play from
  any of them.
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
| Enter | At the end of a file: play the next one |
| P | Queue |
| F, F11 | Fullscreen |
| Ctrl+S | Save the frame to ~/Pictures/mpv-extended |
| Ctrl+O / Ctrl+Shift+O / Ctrl+V | Open files / a folder / a copied link |
| Arrows, Enter, Delete | In the library: pick, play, remove |
| Ctrl+N | New playlist |
| Esc | Close, leave fullscreen, or go to the library |
| ? | All shortcuts |
| Q | Quit |

## Install

### Arch and Omarchy

```sh
sudo pacman -S --needed git base-devel
git clone https://github.com/akshad135/mpv-extended
cd mpv-extended
bin/install          # builds the package and installs it with pacman (asks for sudo)
```

It then shows up in the app launcher, and `mpv-extended [file|folder|url]`
works from a terminal.

### Other distributions: AppImage

Download `mpv-extended-<version>-x86_64.AppImage` from the
[latest release](https://github.com/akshad135/mpv-extended/releases/latest),
then:

```sh
chmod +x mpv-extended-*-x86_64.AppImage
./mpv-extended-*-x86_64.AppImage ~/Videos
```

It carries its own Qt, libmpv, and ffmpeg, and runs on Ubuntu 24.04 and
distributions at least as new (glibc 2.39). From the system it uses:

- the graphics drivers, and libva for hardware decoding;
- an xdg-desktop-portal with a file chooser, for the Open dialogs;
- `yt-dlp`, if installed, to play links.

Each release lists its SHA-256 sums in a `.sha256` file beside the AppImage.
The AppImage isn't added to the app launcher by itself; a tool such as
Gear Lever or AppImageLauncher does that.

## Build

Needs `base-devel qt6-base qt6-declarative mpv ffmpeg wayland-protocols`.
The file picker needs an xdg-desktop-portal backend that provides a file
chooser, such as `xdg-desktop-portal-gtk` (Omarchy has it).

```sh
bin/build                   # build/mpv-extended (CONFIG+=dev adds a screenshot hook for testing)
bin/test                    # unit tests, offscreen
bin/appimage                # dist/mpv-extended-<version>-<arch>.AppImage
build/mpv-extended ~/Videos # play a folder
```

It uses one Qt platform header (to keep the screen awake on Wayland), so
rebuild after a Qt minor upgrade, as with other Qt apps from the AUR.

History lives in `~/.local/share/mpv-extended/history.json` and playlists in
`playlists.json` beside it; frames are cached in `~/.cache/mpv-extended`.

With hardware decoding (on by default), mpv can't copy the frame out for a
capture, so Ctrl+S saves it with ffmpeg instead: the picture is exact, but
without subtitles.

## Releasing

1. Set the version in `mpv-extended.pro` and `pkgbuild/PKGBUILD`.
2. Tag `v<version>` and push the tag to GitHub. The Build workflow runs the
   tests, builds the AppImage, and publishes a GitHub release with it. The
   workflow stops if the tag and the two versions don't match.
3. In `pkgbuild/`, run `updpkgsums` and `makepkg --printsrcinfo > .SRCINFO`.

Pushes to main and pull requests run the same workflow without the release
step; the AppImage it builds is kept as a downloadable artifact of the run.

## License

MIT. See [LICENSE](LICENSE).
