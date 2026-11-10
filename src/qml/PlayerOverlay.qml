import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Everything drawn over the video: the title bar, the control dock, menus,
// feedback for keys and clicks, and what comes next at the end of a file.
Item {
    id: overlay
    required property var player
    required property var app

    property bool playlistOpen: false
    property bool upNextCancelled: false
    property bool showRemaining: false
    readonly property bool menusOpen: tracksSheet.opened || speedSheet.opened || playlistOpen
    readonly property bool controlsShown: player.paused || idleTimer.running || menusOpen
                                          || dockHover.hovered || topHover.hovered || timeline.scrubbing
                                          || upNext.available
    readonly property var audioTracks: player.tracks.filter(t => t.type === "audio")
    readonly property var subtitleTracks: player.tracks.filter(t => t.type === "sub")
    readonly property bool hasPlaylist: player.playlist.length > 1

    function wake() { idleTimer.restart() }
    function closeMenus() {
        if (!menusOpen) return false
        tracksSheet.close(); speedSheet.close(); playlistOpen = false
        return true
    }
    function togglePause() {
        pulse.show(player.paused ? "play" : "pause")
        player.togglePause()
        wake()
    }
    function seekBy(seconds) {
        player.seekRelative(seconds)
        seekChip.add(seconds)
        wake()
    }
    function setVolume(v) {
        player.set("volume", Math.round(Math.max(0, Math.min(130, v))))
        if (player.muted) player.set("mute", false)
        app.toast(v <= 0 ? "muted" : "volume", qsTr("Volume %1%").arg(Math.round(Math.max(0, Math.min(130, v)))))
    }
    function setSpeed(s) {
        player.set("speed", Math.max(0.25, Math.min(4, s)))
        app.toast("speed", qsTr("Speed %1×").arg(Number(Math.max(0.25, Math.min(4, s))).toLocaleString(Qt.locale(), 'f', 2).replace(/\.?0+$/, "")))
    }
    function trackLabel(t) {
        const parts = []
        if (t.lang) parts.push(library.languageName(t.lang))
        if (t.title && t.title !== t.lang) parts.push(t.title)
        if (parts.length === 0) parts.push(qsTr("Track %1").arg(t.id))
        return parts.join(" · ")
    }
    function trackDetail(t) {
        const parts = []
        if (t.forced) parts.push(qsTr("Forced"))
        if (t.external) parts.push(qsTr("File"))
        if (t["demux-channel-count"]) parts.push(t["demux-channel-count"] + "ch")
        if (t.codec) parts.push(String(t.codec).toUpperCase().replace("SUBRIP", "SRT"))
        return parts.join("  ")
    }
    function cycle(kind) {
        player.command(["cycle", kind === "audio" ? "aid" : "sid"])
        const current = player.get(kind === "audio" ? "current-tracks/audio" : "current-tracks/sub")
        if (kind === "audio")
            app.toast("audio", current ? qsTr("Audio: %1").arg(trackLabel(current)) : qsTr("Audio off"))
        else
            app.toast("subtitles", current ? qsTr("Subtitles: %1").arg(trackLabel(current)) : qsTr("Subtitles off"))
    }
    function openTracks() { speedSheet.close(); tracksSheet.opened ? tracksSheet.close() : tracksSheet.open() }
    function togglePlaylist() { tracksSheet.close(); speedSheet.close(); playlistOpen = !playlistOpen }
    function nameOf(entry) {
        if (!entry) return ""
        if (entry.title) return entry.title
        const file = String(entry.filename || "")
        const base = file.split("/").pop()
        return file.includes("://") && base.length === 0 ? file : base.replace(/\.[^.]+$/, "")
    }

    Connections {
        target: overlay.player
        function onFileLoaded() { overlay.upNextCancelled = false; overlay.wake() }
    }

    Timer { id: idleTimer; interval: 2600 }

    // Pointer activity, clicks on the picture, and the wheel for volume.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: overlay.controlsShown ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: overlay.wake()
        onClicked: {
            if (overlay.closeMenus()) return
            overlay.togglePause()
        }
        onDoubleClicked: {
            overlay.player.togglePause()
            overlay.app.toggleFullscreen()
        }
        onWheel: (wheel) => {
            overlay.setVolume(overlay.player.volume + (wheel.angleDelta.y > 0 ? 5 : -5))
            overlay.wake()
        }
    }

    // Audio without artwork gets a calm card instead of a black window.
    Column {
        visible: !overlay.app.hasVideo && !overlay.player.idle && overlay.player.duration > 0
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -40
        spacing: 18
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 200; height: 200
            radius: theme.radius
            gradient: Gradient {
                GradientStop { position: 0; color: theme.mix(theme.surface, theme.accent, 0.22) }
                GradientStop { position: 1; color: theme.surface }
            }
            Glyph { anchors.centerIn: parent; width: 72; height: 72; name: "music"; color: theme.accent; lineWidth: 1.4 }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(overlay.width - 64, 560)
            horizontalAlignment: Text.AlignHCenter
            text: overlay.player.metadata.title || overlay.player.metadata.TITLE || overlay.player.title
            color: theme.strong
            font.pixelSize: 22
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: [overlay.player.metadata.artist || overlay.player.metadata.ARTIST || "",
                   overlay.player.metadata.album || overlay.player.metadata.ALBUM || ""].filter(s => s).join(" — ")
            color: theme.dim
            font.pixelSize: 14
        }
    }

    // Top bar: back, title, and where this file sits in the queue.
    Item {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 84
        opacity: overlay.controlsShown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        HoverHandler { id: topHover }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: theme.alpha(theme.deep, 0.78) }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        IconButton {
            id: backButton
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.top: parent.top
            anchors.topMargin: 14
            icon: "back"
            tip: qsTr("Library (Esc)")
            onClicked: overlay.app.goHome()
        }
        Column {
            anchors.left: backButton.right
            anchors.leftMargin: 10
            anchors.right: helpButton.left
            anchors.rightMargin: 16
            anchors.verticalCenter: backButton.verticalCenter
            spacing: 2
            Text {
                width: parent.width
                text: overlay.player.title
                color: theme.strong
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: overlay.hasPlaylist ? qsTr("%1 of %2").arg(overlay.player.playlistPos + 1).arg(overlay.player.playlist.length)
                                           + (overlay.player.chapter >= 0 && overlay.player.chapters.length > 0
                                              ? "  ·  " + (overlay.player.chapters[overlay.player.chapter].title || qsTr("Chapter %1").arg(overlay.player.chapter + 1)) : "")
                                          : (overlay.player.chapter >= 0 && overlay.player.chapters.length > 0
                                             ? (overlay.player.chapters[overlay.player.chapter].title || qsTr("Chapter %1").arg(overlay.player.chapter + 1)) : "")
                color: theme.dim
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
        IconButton {
            id: helpButton
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.top: parent.top
            anchors.topMargin: 14
            icon: "help"
            tip: qsTr("Shortcuts (?)")
            onClicked: overlay.app.helpVisible = !overlay.app.helpVisible
        }
    }

    // Big play/pause pulse where the click landed: the center.
    Rectangle {
        id: pulse
        property string icon: "play"
        function show(name) { icon = name; pulseAnimation.restart() }
        anchors.centerIn: parent
        width: 88; height: 88; radius: 44
        color: theme.alpha(theme.deep, 0.6)
        opacity: 0
        Glyph { anchors.centerIn: parent; anchors.horizontalCenterOffset: pulse.icon === "play" ? 3 : 0; width: 38; height: 38; name: pulse.icon; color: theme.strong }
        ParallelAnimation {
            id: pulseAnimation
            NumberAnimation { target: pulse; property: "opacity"; from: 1; to: 0; duration: 650; easing.type: Easing.InQuad }
            NumberAnimation { target: pulse; property: "scale"; from: 0.85; to: 1.3; duration: 650; easing.type: Easing.OutCubic }
        }
    }

    // "+10s" on the side the seek went, adding up while keys repeat.
    Rectangle {
        id: seekChip
        property real total: 0
        function add(seconds) {
            total = (fade.running || opacity > 0) && Math.sign(total) === Math.sign(seconds) ? total + seconds : seconds
            opacity = 1
            fade.restart()
        }
        anchors.verticalCenter: parent.verticalCenter
        x: total >= 0 ? parent.width * 0.75 - width / 2 : parent.width * 0.25 - width / 2
        width: chipRow.implicitWidth + 28
        height: 44
        radius: Math.min(theme.radius, 22)
        color: theme.alpha(theme.deep, 0.66)
        opacity: 0
        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 6
            Glyph { name: seekChip.total >= 0 ? "forward" : "rewind"; width: 18; height: 18; color: theme.strong; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: (seekChip.total >= 0 ? "+" : "−") + Math.abs(seekChip.total) + "s"
                color: theme.strong
                font.pixelSize: 15
                font.weight: Font.DemiBold
                font.family: "monospace"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        SequentialAnimation {
            id: fade
            PauseAnimation { duration: 600 }
            NumberAnimation { target: seekChip; property: "opacity"; to: 0; duration: 300 }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: overlay.player.buffering || (!overlay.player.idle && overlay.player.duration <= 0 && overlay.player.path.length > 0)
        visible: running
    }

    // The end of the last file: watch again or go back.
    Row {
        id: endActions
        visible: overlay.player.eof && !upNext.available
        anchors.centerIn: parent
        spacing: 12
        Repeater {
            model: [
                { icon: "replay", text: qsTr("Watch again"), action: () => overlay.togglePause() },
                { icon: "back", text: qsTr("Library"), action: () => overlay.app.goHome() },
            ]
            Rectangle {
                required property var modelData
                required property int index
                width: endRow.implicitWidth + 32
                height: 44
                radius: Math.min(theme.radius, 22)
                color: index === 0 ? theme.accent : (endArea.containsMouse ? theme.alpha(theme.background, 0.95) : theme.alpha(theme.background, 0.82))
                border.color: index === 0 ? "transparent" : theme.alpha(theme.foreground, 0.14)
                Row {
                    id: endRow
                    anchors.centerIn: parent
                    spacing: 8
                    Glyph { name: modelData.icon; width: 18; height: 18; color: index === 0 ? theme.accentText : theme.strong; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: modelData.text; color: index === 0 ? theme.accentText : theme.strong; font.pixelSize: 14; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
                }
                MouseArea { id: endArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: modelData.action() }
            }
        }
    }

    // Bottom dock. The video behind it is blurred so it reads on any frame.
    Item {
        id: dock
        width: Math.min(parent.width - 32, 1120)
        height: 96
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height - height - (overlay.controlsShown ? 16 : 0)
        opacity: overlay.controlsShown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        HoverHandler { id: dockHover }

        ShaderEffectSource {
            id: behind
            sourceItem: overlay.player
            sourceRect: Qt.rect(dock.x, dock.y, dock.width, dock.height)
            live: true
            visible: false
            recursive: false
        }
        Rectangle {
            id: dockMask
            width: dock.width
            height: dock.height
            radius: theme.radius
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: behind
            blurEnabled: true
            blur: 1.0
            blurMax: 48
            saturation: -0.2
            maskEnabled: true
            maskSource: dockMask
        }
        Rectangle {
            anchors.fill: parent
            radius: theme.radius
            color: theme.alpha(theme.background, 0.74)
            border.color: theme.alpha(theme.foreground, 0.1)
            border.width: 1
        }

        Timeline {
            id: timeline
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.topMargin: 10
            position: overlay.player.position
            duration: overlay.player.duration
            cacheEnd: overlay.player.cacheEnd
            chapters: overlay.player.chapters
            onSeekRequested: (seconds, exact) => { overlay.player.seek(seconds, exact); overlay.wake() }
        }

        Row {
            id: leftControls
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            spacing: 2
            IconButton {
                visible: overlay.hasPlaylist
                icon: "previous"; size: 40
                enabledLook: overlay.player.playlistPos > 0
                tip: qsTr("Previous (Shift+N)")
                onClicked: overlay.app.playPrevious()
            }
            IconButton {
                icon: overlay.player.paused ? "play" : "pause"
                size: 44
                glyphSize: 24
                tip: overlay.player.paused ? qsTr("Play (Space)") : qsTr("Pause (Space)")
                onClicked: overlay.togglePause()
            }
            IconButton {
                visible: overlay.hasPlaylist
                icon: "next"; size: 40
                enabledLook: overlay.player.playlistPos < overlay.player.playlist.length - 1
                tip: qsTr("Next (N)")
                onClicked: overlay.app.playNext()
            }
            Item { width: 10; height: 1 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: (overlay.showRemaining ? "−" + library.formatTime(overlay.player.duration - timeline.shown)
                                             : library.formatTime(timeline.shown))
                      + "  /  " + library.formatTime(overlay.player.duration)
                color: theme.foreground
                font.pixelSize: 13
                font.family: "monospace"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: overlay.showRemaining = !overlay.showRemaining
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            spacing: 2

            IconButton {
                id: volumeButton
                size: 40
                icon: overlay.player.muted || overlay.player.volume <= 0 ? "muted"
                      : overlay.player.volume < 50 ? "volume-low" : "volume"
                tip: qsTr("Mute (M)")
                onClicked: overlay.player.set("mute", !overlay.player.muted)
            }
            // Volume: drag or click anywhere on the bar.
            Item {
                width: 86
                height: 40
                anchors.verticalCenter: parent.verticalCenter
                readonly property real level: overlay.player.muted ? 0 : Math.min(1, overlay.player.volume / 100)
                Rectangle {
                    id: volumeTrack
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 4
                    radius: 2
                    color: theme.alpha(theme.foreground, 0.18)
                    Rectangle {
                        width: parent.width * parent.parent.level
                        height: parent.height
                        radius: 2
                        color: overlay.player.volume > 100 ? theme.yellow : theme.strong
                    }
                }
                Rectangle {
                    width: 12; height: 12; radius: 6
                    color: theme.strong
                    anchors.verticalCenter: parent.verticalCenter
                    x: parent.width * parent.level - width / 2
                    scale: volumeArea.containsMouse || volumeArea.pressed ? 1 : 0
                    Behavior on scale { NumberAnimation { duration: 120 } }
                }
                MouseArea {
                    id: volumeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.PointingHandCursor
                    function apply(x) {
                        overlay.player.set("volume", Math.round(Math.max(0, Math.min(1, x / width)) * 100))
                        if (overlay.player.muted) overlay.player.set("mute", false)
                    }
                    onPressed: (mouse) => apply(mouse.x)
                    onPositionChanged: (mouse) => { if (pressed) apply(mouse.x) }
                }
            }
            Item { width: 8; height: 1 }

            // Speed reads as text so a non-default rate is visible at a glance.
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(40, speedLabel.implicitWidth + 16)
                height: 30
                radius: Math.min(4, theme.radius)
                color: speedArea.containsMouse || speedSheet.opened ? theme.alpha(theme.foreground, 0.09) : "transparent"
                border.color: overlay.player.speed !== 1 ? theme.accent : "transparent"
                Text {
                    id: speedLabel
                    anchors.centerIn: parent
                    text: Number(overlay.player.speed).toLocaleString(Qt.locale(), 'f', 2).replace(/[.,]?0+$/, "") + "×"
                    color: overlay.player.speed !== 1 ? theme.accent : theme.strong
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    font.family: "monospace"
                }
                MouseArea {
                    id: speedArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { tracksSheet.close(); speedSheet.opened ? speedSheet.close() : speedSheet.open() }
                }
                ToolTip.visible: speedArea.containsMouse
                ToolTip.text: qsTr("Speed ([ and ])")
                ToolTip.delay: 600
            }
            IconButton {
                size: 40
                icon: "subtitles"
                active: tracksSheet.opened
                tip: qsTr("Audio and subtitles (T)")
                onClicked: overlay.openTracks()
            }
            IconButton {
                visible: overlay.hasPlaylist
                size: 40
                icon: "playlist"
                active: overlay.playlistOpen
                tip: qsTr("Queue (P)")
                onClicked: overlay.togglePlaylist()
            }
            IconButton {
                size: 40
                icon: overlay.app.fullscreen ? "windowed" : "fullscreen"
                tip: qsTr("Fullscreen (F)")
                onClicked: overlay.app.toggleFullscreen()
            }
        }
    }

    // What plays next, counting down at the end of a file.
    Rectangle {
        id: upNext
        readonly property bool available: overlay.player.eof && overlay.hasPlaylist && !overlay.upNextCancelled
                                          && overlay.player.playlistPos < overlay.player.playlist.length - 1
        readonly property var entry: available ? overlay.player.playlist[overlay.player.playlistPos + 1] : null
        property real progress: 0
        onAvailableChanged: available ? countdown.restart() : countdown.stop()

        anchors.right: parent.right
        anchors.rightMargin: 24
        y: dock.y - height - 16
        width: Math.min(360, parent.width - 48)
        height: 104
        radius: theme.radius
        color: theme.alpha(theme.background, 0.95)
        border.color: theme.alpha(theme.foreground, 0.12)
        opacity: available ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        NumberAnimation {
            id: countdown
            target: upNext
            property: "progress"
            from: 0; to: 1
            duration: 6000
            onFinished: if (upNext.available) overlay.app.playNext()
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 3
            Text { text: qsTr("Up next"); color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.8 }
            Text { width: parent.width; text: overlay.nameOf(upNext.entry); color: theme.strong; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideMiddle }
        }
        Row {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 8
            Rectangle {
                width: playNow.implicitWidth + 24; height: 30
                radius: Math.min(4, theme.radius)
                color: theme.accent
                clip: true
                Rectangle { width: parent.width * upNext.progress; height: parent.height; color: theme.alpha(theme.accentText, 0.14) }
                Text { id: playNow; anchors.centerIn: parent; text: qsTr("Play now  ↵"); color: theme.accentText; font.pixelSize: 12; font.weight: Font.DemiBold }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: overlay.app.playNext() }
            }
            Rectangle {
                width: cancelText.implicitWidth + 24; height: 30
                radius: Math.min(4, theme.radius)
                color: cancelArea.containsMouse ? theme.alpha(theme.foreground, 0.1) : "transparent"
                border.color: theme.alpha(theme.foreground, 0.16)
                Text { id: cancelText; anchors.centerIn: parent; text: qsTr("Cancel"); color: theme.foreground; font.pixelSize: 12 }
                MouseArea { id: cancelArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: overlay.upNextCancelled = true }
            }
        }
    }

    PlaylistPanel {
        id: playlistPanel
        player: overlay.player
        open: overlay.playlistOpen
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: dock.height + 32
        width: Math.min(360, parent.width - 32)
        x: open ? parent.width - width - 16 : parent.width + 8
        Behavior on x { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
        onPicked: (index) => overlay.player.playIndex(index)
        onCloseRequested: overlay.playlistOpen = false
    }

    Sheet {
        id: speedSheet
        parent: overlay
        width: 168
        x: dock.x + dock.width - width - 150
        y: dock.y - height - 10
        contentItem: Column {
            spacing: 0
            Repeater {
                model: [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 3]
                MenuRow {
                    required property var modelData
                    text: Number(modelData).toLocaleString(Qt.locale(), 'f', 2).replace(/[.,]?0+$/, "") + "×"
                    detail: modelData === 1 ? qsTr("Normal") : ""
                    checked: Math.abs(overlay.player.speed - modelData) < 0.001
                    onClicked: { overlay.player.set("speed", modelData); speedSheet.close() }
                }
            }
        }
    }

    Sheet {
        id: tracksSheet
        parent: overlay
        width: Math.min(600, overlay.width - 32)
        x: Math.max(16, dock.x + dock.width - width - 8)
        y: dock.y - height - 10
        contentItem: Flickable {
            implicitHeight: Math.min(columns.implicitHeight, overlay.height * 0.55)
            contentHeight: columns.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Row {
                id: columns
                width: parent.width
                spacing: 8
                Column {
                    width: (columns.width - 8) / 2
                    Text { text: qsTr("Audio"); color: theme.dim; font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.8; leftPadding: 10; topPadding: 6; bottomPadding: 6 }
                    Repeater {
                        model: overlay.audioTracks
                        MenuRow {
                            required property var modelData
                            text: overlay.trackLabel(modelData)
                            detail: overlay.trackDetail(modelData)
                            checked: modelData.selected === true
                            onClicked: overlay.player.set("aid", modelData.id)
                        }
                    }
                    MenuRow {
                        visible: overlay.audioTracks.length === 0
                        text: qsTr("No audio")
                        enabled: false
                    }
                }
                Column {
                    width: (columns.width - 8) / 2
                    Text { text: qsTr("Subtitles"); color: theme.dim; font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.8; leftPadding: 10; topPadding: 6; bottomPadding: 6 }
                    MenuRow {
                        text: qsTr("Off")
                        checked: !overlay.subtitleTracks.some(t => t.selected === true)
                        onClicked: overlay.player.set("sid", "no")
                    }
                    Repeater {
                        model: overlay.subtitleTracks
                        MenuRow {
                            required property var modelData
                            text: overlay.trackLabel(modelData)
                            detail: overlay.trackDetail(modelData)
                            checked: modelData.selected === true
                            onClicked: overlay.player.set("sid", modelData.id)
                        }
                    }
                    MenuRow {
                        text: qsTr("Load from file…")
                        onClicked: { tracksSheet.close(); picker.openSubtitle() }
                    }
                }
            }
        }
    }
}
