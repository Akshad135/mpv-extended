import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window
import Reel

ApplicationWindow {
    id: win
    width: 1200
    height: 740
    minimumWidth: 600
    minimumHeight: 380
    visible: true
    title: inPlayer && mpv.title ? mpv.title + " — Reel" : "Reel"
    color: theme.deep

    Material.theme: theme.isLight ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.primary: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    readonly property bool inPlayer: !mpv.idle
    readonly property bool fullscreen: win.visibility === Window.FullScreen
    // A cover image counts as a picture to show, but not as video to keep
    // the screen awake for.
    readonly property bool hasPicture: mpv.tracks.some(t => t.type === "video")
    readonly property bool hasVideo: mpv.tracks.some(t => t.type === "video" && !t.albumart)
    readonly property bool dropActive: drop.containsDrag
    property bool helpVisible: false

    function toast(icon, text) { toastItem.show(icon, text, false) }
    function showError(text) { toastItem.show("warning", text, true) }

    function toggleFullscreen() { fullscreen ? win.showNormal() : win.showFullScreen() }

    function openInputs(list) {
        const result = library.expand(list)
        if (result.files.length === 0) {
            showError(qsTr("Nothing playable there."))
            return
        }
        helpVisible = false
        mpv.loadFiles(result.files, result.index)
    }

    function openClipboard() {
        const text = library.clipboardText()
        if (/^[a-z][a-z0-9+.-]*:\/\//i.test(text) || text.startsWith("/"))
            openInputs([text])
        else
            showError(qsTr("The clipboard has no link or file path."))
    }

    // Progress is written as playback goes, so a crash or a closed window
    // loses seconds at most. A frame is kept for the library card; it is
    // never waited for, since mpv may need the render thread to take it.
    function saveProgress(withFrame) {
        if (!inPlayer || !mpv.path || mpv.duration <= 0) return
        library.update(mpv.path, mpv.title, mpv.position, mpv.duration)
        if (withFrame && hasPicture && mpv.position > 1)
            mpv.screenshotTo(library.thumbnailFile(mpv.path), mpv.path)
    }

    function goHome() {
        saveProgress(true)
        if (fullscreen) win.showNormal()
        overlay.closeMenus()
        mpv.stop()
    }

    function playNext() {
        if (mpv.playlistPos < mpv.playlist.length - 1) {
            saveProgress(true)
            mpv.playIndex(mpv.playlistPos + 1)
        }
    }
    function playPrevious() {
        // Like a CD player: past the first few seconds, back goes to the start.
        if (mpv.position > 5 || mpv.playlistPos <= 0) { mpv.seek(0, true); return }
        saveProgress(true)
        mpv.playIndex(mpv.playlistPos - 1)
    }

    function handleEscape() {
        if (helpVisible) helpVisible = false
        else if (overlay.closeMenus()) {}
        else if (fullscreen) win.showNormal()
        else if (inPlayer) goHome()
    }

    function captureFrame() {
        if (!hasPicture) return
        const file = library.frameCaptureFile(mpv.title, mpv.position)
        mpv.screenshotTo(file, "capture:" + file, true)
    }

    // Closing only records the position; the library card keeps the frame
    // saved on the last pause or within the last 20 seconds.
    onClosing: { saveProgress(false); library.save() }

    Component.onCompleted: {
        mpv.set("background-color", theme.deep)
        if (initialInputs.length > 0) openInputs(initialInputs)
    }

    Connections {
        target: theme
        function onChanged() { mpv.set("background-color", theme.deep) }
    }
    Connections {
        target: mpv
        function onFailed(path, reason) {
            const name = String(path).split("/").pop() || path
            win.showError(qsTr("Can't play %1: %2").arg(name).arg(reason))
        }
        function onResumed(position) { win.toast("replay", qsTr("Resumed at %1").arg(library.formatTime(position))) }
        function onScreenshotSaved(tag) {
            if (tag.startsWith("capture:"))
                win.toast("camera", qsTr("Frame saved to %1").arg(tag.slice(8).replace(/^.*\/(Pictures\/)/, "$1")))
            else if (tag)
                library.thumbnailUpdated(tag)
        }
        function onPausedChanged() { if (mpv.paused) win.saveProgress(true) }
        function onEofChanged() { if (mpv.eof) win.saveProgress(false) }
        function onFileLoaded() { thumbnailer.prepare(mpv.path, mpv.duration) }
        function onDurationChanged() { if (mpv.duration > 0) thumbnailer.prepare(mpv.path, mpv.duration) }
    }
    Connections {
        target: picker
        function onPicked(urls, purpose) {
            if (purpose === "subtitle") {
                mpv.command(["sub-add", urls[0], "select"])
                win.toast("subtitles", qsTr("Subtitles loaded"))
            } else {
                win.openInputs(urls)
            }
        }
        function onFailed(message) { win.showError(message) }
    }

    Timer {
        interval: 3000
        repeat: true
        running: win.inPlayer && !mpv.paused
        onTriggered: win.saveProgress(false)
    }
    Timer {
        interval: 20000
        repeat: true
        running: win.inPlayer && !mpv.paused
        onTriggered: win.saveProgress(true)
    }

    Binding { target: inhibitor; property: "active"; value: win.inPlayer && !mpv.paused && win.hasVideo }

    // Always visible, even under the library, so its render context exists
    // before the first file opens.
    MpvVideo {
        id: mpv
        anchors.fill: parent
    }

    PlayerOverlay {
        id: overlay
        anchors.fill: parent
        player: mpv
        app: win
        visible: win.inPlayer
    }

    HomeView {
        id: home
        anchors.fill: parent
        app: win
        focus: !win.inPlayer
        opacity: win.inPlayer ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (event) => {
            if (event.hasUrls) {
                win.openInputs(event.urls.map(u => u.toString()))
                event.acceptProposedAction()
            }
        }
    }
    // Drop target over the player, so dropping onto a playing video reads
    // as "play this instead".
    Rectangle {
        anchors.fill: parent
        anchors.margins: 12
        visible: drop.containsDrag && (win.inPlayer || !home.empty)
        radius: theme.radius
        color: theme.alpha(theme.deep, 0.7)
        border.color: theme.accent
        border.width: 2
        Column {
            anchors.centerIn: parent
            spacing: 12
            Glyph { anchors.horizontalCenter: parent.horizontalCenter; width: 44; height: 44; name: "plus"; color: theme.accent }
            Text { text: qsTr("Drop to play"); color: theme.strong; font.pixelSize: 18; font.weight: Font.DemiBold }
        }
    }

    HelpOverlay {
        anchors.fill: parent
        shown: win.helpVisible
        onDismissed: win.helpVisible = false
    }

    Toast {
        id: toastItem
        anchors.horizontalCenter: parent.horizontalCenter
        y: 22
    }

    // Shortcuts. The player's are off in the library, where arrows move the
    // selection instead.
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; onActivated: win.handleEscape() }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; onActivated: win.toggleFullscreen() }
    Shortcut { sequence: "F11"; context: Qt.ApplicationShortcut; onActivated: win.toggleFullscreen() }
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: picker.openFiles() }
    Shortcut { sequence: "Ctrl+Shift+O"; context: Qt.ApplicationShortcut; onActivated: picker.openFolder() }
    Shortcut { sequences: [StandardKey.Paste]; context: Qt.ApplicationShortcut; onActivated: win.openClipboard() }

    Shortcut { enabled: win.inPlayer; sequences: ["Space", "K"]; context: Qt.ApplicationShortcut; onActivated: overlay.togglePause() }
    Shortcut { enabled: win.inPlayer; sequence: "Left"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(-5) }
    Shortcut { enabled: win.inPlayer; sequence: "Right"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(5) }
    Shortcut { enabled: win.inPlayer; sequence: "Shift+Left"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(-1) }
    Shortcut { enabled: win.inPlayer; sequence: "Shift+Right"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(1) }
    Shortcut { enabled: win.inPlayer; sequence: "J"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(-10) }
    Shortcut { enabled: win.inPlayer; sequence: "L"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(10) }
    Shortcut { enabled: win.inPlayer; sequence: "Up"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.setVolume(mpv.volume + 5) }
    Shortcut { enabled: win.inPlayer; sequence: "Down"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.setVolume(mpv.volume - 5) }
    Shortcut {
        enabled: win.inPlayer; sequence: "M"; context: Qt.ApplicationShortcut
        onActivated: { mpv.set("mute", !mpv.muted); win.toast(mpv.muted ? "volume" : "muted", mpv.muted ? qsTr("Sound on") : qsTr("Muted")) }
    }
    Shortcut { enabled: win.inPlayer; sequence: ","; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: mpv.command(["frame-back-step"]) }
    Shortcut { enabled: win.inPlayer; sequence: "."; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: mpv.command(["frame-step"]) }
    Shortcut { enabled: win.inPlayer; sequence: "["; context: Qt.ApplicationShortcut; onActivated: overlay.setSpeed(mpv.speed - 0.25) }
    Shortcut { enabled: win.inPlayer; sequence: "]"; context: Qt.ApplicationShortcut; onActivated: overlay.setSpeed(mpv.speed + 0.25) }
    Shortcut { enabled: win.inPlayer; sequence: "Backspace"; context: Qt.ApplicationShortcut; onActivated: overlay.setSpeed(1) }
    Shortcut { enabled: win.inPlayer; sequence: "A"; context: Qt.ApplicationShortcut; onActivated: overlay.cycle("audio") }
    Shortcut { enabled: win.inPlayer; sequence: "S"; context: Qt.ApplicationShortcut; onActivated: overlay.cycle("sub") }
    Shortcut { enabled: win.inPlayer; sequence: "T"; context: Qt.ApplicationShortcut; onActivated: overlay.openTracks() }
    Shortcut { enabled: win.inPlayer; sequence: "P"; context: Qt.ApplicationShortcut; onActivated: overlay.togglePlaylist() }
    Shortcut { enabled: win.inPlayer; sequence: "N"; context: Qt.ApplicationShortcut; onActivated: win.playNext() }
    Shortcut { enabled: win.inPlayer; sequence: "Shift+N"; context: Qt.ApplicationShortcut; onActivated: win.playPrevious() }
    Shortcut { enabled: win.inPlayer; sequence: "Return"; context: Qt.ApplicationShortcut; onActivated: if (mpv.eof) win.playNext() }
    Shortcut { enabled: win.inPlayer; sequence: "PgUp"; context: Qt.ApplicationShortcut; onActivated: mpv.command(["add", "chapter", -1]) }
    Shortcut { enabled: win.inPlayer; sequence: "PgDown"; context: Qt.ApplicationShortcut; onActivated: mpv.command(["add", "chapter", 1]) }
    Shortcut { enabled: win.inPlayer; sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: win.captureFrame() }
    Instantiator {
        model: 10
        delegate: Shortcut {
            required property int index
            enabled: win.inPlayer
            sequence: String(index)
            context: Qt.ApplicationShortcut
            onActivated: { mpv.seek(mpv.duration * index / 10, false); overlay.wake() }
        }
    }
}
