import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window
import MpvExtended

ApplicationWindow {
    id: win
    width: 1200
    height: 740
    minimumWidth: 600
    minimumHeight: 380
    visible: true
    title: inPlayer && mpv.title ? mpv.title + " — mpv-extended" : "mpv-extended"
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
    // Player keys rest while the shortcut sheet is up.
    readonly property bool playerKeys: inPlayer && !helpVisible
    onInPlayerChanged: if (!inPlayer) library.refresh()

    // Fills %1, %2, ... in one pass, so a file name that contains "%2"
    // stays as it is (chained .arg() would substitute into it).
    function fill(template, values) {
        return template.replace(/%(\d)/g, (match, n) => n >= 1 && n <= values.length ? String(values[n - 1]) : match)
    }
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
        // Whatever was playing keeps its place, and a switch or a trip to the
        // library that was still waiting for its frame is called off.
        saveProgress(true)
        ++navSerial
        switchTarget = -1
        goingHome = false
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
    // loses seconds at most. A frame is kept for the library card.
    function saveProgress(withFrame) {
        if (!inPlayer || !mpv.path || mpv.duration <= 0) return
        library.update(mpv.path, mpv.title, mpv.position, mpv.duration)
        if (withFrame) grabFrame(null)
    }

    // mpv takes screenshots on its own thread, so a stop or a file switch
    // sent right after one can win. Leaving a file therefore waits for the
    // frame, briefly, without ever blocking the window.
    property int frameSerial: 0
    property var frameJobs: ({})
    function grabFrame(then) {
        const path = mpv.path
        if (!hasPicture || !path || mpv.position <= 1) { if (then) then(); return }
        const tag = "frame:" + (++frameSerial)
        frameJobs[tag] = { path: path, then: then }
        mpv.screenshotTo(library.thumbnailFile(path), tag)
        if (then) frameTimeout.restart()
    }
    function finishFrame(tag, ok) {
        const job = frameJobs[tag]
        if (!job) return
        delete frameJobs[tag]
        if (ok) {
            library.thumbnailUpdated(job.path)
            if (!inPlayer) library.refresh()
        }
        if (job.then) job.then()
    }
    Timer {
        id: frameTimeout
        interval: 500
        onTriggered: {
            // Too slow: go on, and let the frame land in the library later.
            for (const tag in win.frameJobs) {
                const job = win.frameJobs[tag]
                if (job.then) { const then = job.then; job.then = null; then() }
            }
        }
    }

    function leaveFile(then) {
        if (inPlayer && mpv.path && mpv.duration > 0)
            library.update(mpv.path, mpv.title, mpv.position, mpv.duration)
        grabFrame(then)
    }

    // While a file is being left, the frame wait is still running. Further
    // N presses move the target instead of starting a second switch, and
    // nothing switches files once the library is on its way.
    property int switchTarget: -1
    property int navSerial: 0
    property bool goingHome: false
    readonly property int targetPos: switchTarget >= 0 ? switchTarget : mpv.playlistPos

    function goHome() {
        if (goingHome) return
        if (fullscreen) win.showNormal()
        overlay.closeMenus()
        goingHome = true
        switchTarget = -1
        const serial = ++navSerial
        mpv.set("pause", true)
        leaveFile(() => {
            if (serial !== navSerial) return
            goingHome = false
            mpv.stop()
        })
    }

    function playAt(index) {
        if (goingHome || index < 0 || index >= mpv.playlist.length) return
        if (switchTarget >= 0) { switchTarget = index; return }
        if (index === mpv.playlistPos) return
        switchTarget = index
        const serial = ++navSerial
        leaveFile(() => {
            if (serial !== navSerial) return
            const target = switchTarget
            switchTarget = -1
            if (target >= 0 && target !== mpv.playlistPos) mpv.playIndex(target)
        })
    }
    function playNext() { playAt(targetPos + 1) }
    function playPrevious() {
        // Like a CD player: past the first few seconds, back goes to the start.
        if (switchTarget < 0 && (mpv.position > 5 || mpv.playlistPos <= 0)) { mpv.seek(0, true); return }
        playAt(targetPos - 1)
    }

    function prepareFrames() {
        // Only real video gets seek previews; ffmpeg has nothing to grab
        // from audio.
        if (hasVideo && mpv.duration > 0) thumbnailer.prepare(mpv.path, mpv.duration)
        else thumbnailer.prepare("", 0)
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
            win.showError(win.fill(qsTr("Can't play %1: %2"), [name, reason]))
        }
        function onResumed(position) { win.toast("replay", qsTr("Resumed at %1").arg(library.formatTime(position))) }
        function onScreenshotFinished(tag, ok) {
            if (tag.startsWith("capture:")) {
                const file = tag.slice(8)
                if (ok) win.toast("camera", qsTr("Frame saved to %1").arg(file.replace(/^.*\/(Pictures\/)/, "$1")))
                else win.showError(qsTr("Couldn't save the frame to %1").arg(file))
            } else {
                win.finishFrame(tag, ok)
            }
        }
        function onVideoOutputFailed() { win.showError(qsTr("Video can't be shown: OpenGL isn't available.")) }
        // Going home pauses first and saves the frame itself.
        function onPausedChanged() { if (mpv.paused && !win.goingHome) win.saveProgress(true) }
        function onEofChanged() { if (mpv.eof) win.saveProgress(false) }
        function onFileLoaded() { win.prepareFrames() }
        function onDurationChanged() { win.prepareFrames() }
        function onTracksChanged() { win.prepareFrames() }
    }
    Connections {
        target: picker
        function onPicked(urls, purpose) {
            if (purpose === "subtitle") {
                if (mpv.command(["sub-add", urls[0], "select"]))
                    win.toast("subtitles", qsTr("Subtitles loaded"))
                else
                    win.showError(qsTr("Couldn't load those subtitles."))
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
    Shortcut { sequences: ["?", "Shift+?"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; onActivated: win.handleEscape() }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; onActivated: win.toggleFullscreen() }
    Shortcut { sequence: "F11"; context: Qt.ApplicationShortcut; onActivated: win.toggleFullscreen() }
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: picker.openFiles() }
    Shortcut { sequence: "Ctrl+Shift+O"; context: Qt.ApplicationShortcut; onActivated: picker.openFolder() }
    Shortcut { sequences: [StandardKey.Paste]; context: Qt.ApplicationShortcut; onActivated: win.openClipboard() }

    Shortcut { enabled: win.playerKeys; sequences: ["Space", "K"]; context: Qt.ApplicationShortcut; onActivated: overlay.togglePause() }
    Shortcut { enabled: win.playerKeys; sequence: "Left"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(-5) }
    Shortcut { enabled: win.playerKeys; sequence: "Right"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(5) }
    Shortcut { enabled: win.playerKeys; sequence: "Shift+Left"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(-1) }
    Shortcut { enabled: win.playerKeys; sequence: "Shift+Right"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(1) }
    Shortcut { enabled: win.playerKeys; sequence: "J"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(-10) }
    Shortcut { enabled: win.playerKeys; sequence: "L"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.seekBy(10) }
    Shortcut { enabled: win.playerKeys; sequence: "Up"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.setVolume(mpv.volume + 5) }
    Shortcut { enabled: win.playerKeys; sequence: "Down"; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: overlay.setVolume(mpv.volume - 5) }
    Shortcut {
        enabled: win.playerKeys; sequence: "M"; context: Qt.ApplicationShortcut
        onActivated: { mpv.set("mute", !mpv.muted); win.toast(mpv.muted ? "volume" : "muted", mpv.muted ? qsTr("Sound on") : qsTr("Muted")) }
    }
    Shortcut { enabled: win.playerKeys; sequence: ","; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: mpv.command(["frame-back-step"]) }
    Shortcut { enabled: win.playerKeys; sequence: "."; context: Qt.ApplicationShortcut; autoRepeat: true; onActivated: mpv.command(["frame-step"]) }
    Shortcut { enabled: win.playerKeys; sequence: "["; context: Qt.ApplicationShortcut; onActivated: overlay.setSpeed(mpv.speed - 0.25) }
    Shortcut { enabled: win.playerKeys; sequence: "]"; context: Qt.ApplicationShortcut; onActivated: overlay.setSpeed(mpv.speed + 0.25) }
    Shortcut { enabled: win.playerKeys; sequence: "Backspace"; context: Qt.ApplicationShortcut; onActivated: overlay.setSpeed(1) }
    Shortcut { enabled: win.playerKeys; sequence: "A"; context: Qt.ApplicationShortcut; onActivated: overlay.cycle("audio") }
    Shortcut { enabled: win.playerKeys; sequence: "S"; context: Qt.ApplicationShortcut; onActivated: overlay.cycle("sub") }
    Shortcut { enabled: win.playerKeys; sequence: "T"; context: Qt.ApplicationShortcut; onActivated: overlay.openTracks() }
    Shortcut { enabled: win.playerKeys; sequence: "P"; context: Qt.ApplicationShortcut; onActivated: if (overlay.hasPlaylist) overlay.togglePlaylist() }
    Shortcut { enabled: win.playerKeys; sequence: "N"; context: Qt.ApplicationShortcut; onActivated: win.playNext() }
    Shortcut { enabled: win.playerKeys; sequence: "Shift+N"; context: Qt.ApplicationShortcut; onActivated: win.playPrevious() }
    Shortcut { enabled: win.playerKeys; sequence: "Return"; context: Qt.ApplicationShortcut; onActivated: overlay.playUpNext() }
    Shortcut { enabled: win.playerKeys; sequence: "PgUp"; context: Qt.ApplicationShortcut; onActivated: mpv.command(["add", "chapter", -1]) }
    Shortcut { enabled: win.playerKeys; sequence: "PgDown"; context: Qt.ApplicationShortcut; onActivated: mpv.command(["add", "chapter", 1]) }
    Shortcut { enabled: win.playerKeys; sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: win.captureFrame() }
    Instantiator {
        model: 10
        delegate: Shortcut {
            required property int index
            enabled: win.playerKeys && mpv.duration > 0
            sequence: String(index)
            context: Qt.ApplicationShortcut
            onActivated: { mpv.seek(mpv.duration * index / 10, false); overlay.wake() }
        }
    }
}
