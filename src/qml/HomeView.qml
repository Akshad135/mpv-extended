import QtQuick
import QtQuick.Controls
import "Keys.js" as KeyNames

// The library: pick up where you left off, or open something new.
FocusScope {
    id: home
    required property var app
    readonly property var playlistList: playlists.all
    readonly property var continueList: library.continueWatching
    readonly property var recentList: library.recent
    readonly property int total: playlistList.length + continueList.length + recentList.length
    readonly property bool empty: total === 0
    property int selection: -1
    // The playlist open in the editor, if any.
    property string editingId: ""

    function editPlaylist(id) { editingId = id; editor.forceActiveFocus() }
    function newPlaylist() {
        const id = playlists.save(qsTr("New playlist"))
        if (!id) { app.showError(qsTr("Couldn't save the playlist.")); return }
        editPlaylist(id)
        editor.renameNow()
    }
    function closeEditor() { editingId = ""; home.forceActiveFocus() }
    function addToEditor(urls) { editor.add(urls) }
    function escapeEditor() { editor.leave() }

    // The selection runs through the sections in order: two rows of cards,
    // then the list.
    readonly property var sections: [
        { list: playlistList, row: true },
        { list: continueList, row: true },
        { list: recentList, row: false },
    ]
    function start(s) {
        let n = 0
        for (let i = 0; i < s; ++i) n += sections[i].list.length
        return n
    }
    function place(i) {
        for (let s = 0; s < sections.length; ++s) {
            const n = sections[s].list.length
            if (i < n) return { section: s, offset: i }
            i -= n
        }
        return { section: -1, offset: -1 }
    }
    function entryAt(i) {
        const p = place(i)
        return p.section < 0 ? null : sections[p.section].list[p.offset]
    }
    function open(i) {
        const p = place(i), entry = entryAt(i)
        if (!entry) return
        if (p.section === 0) app.openPlaylist(entry.id)
        else app.openInputs([entry.path])
    }
    function remove(i) {
        const p = place(i), entry = entryAt(i)
        if (!entry) return
        if (p.section === 0) playlists.forget(entry.id)
        else library.forget(entry.path)
    }
    function move(key) {
        if (total === 0) return
        if (selection < 0) { selection = 0; reveal(); return }
        const p = place(selection)
        const section = sections[p.section]
        const last = section.list.length - 1
        if (section.row && (key === Qt.Key_Left || key === Qt.Key_Right)) {
            selection = start(p.section) + Math.max(0, Math.min(last, p.offset + (key === Qt.Key_Left ? -1 : 1)))
        } else if (key === Qt.Key_Down) {
            if (!section.row && p.offset < last) ++selection
            else for (let s = p.section + 1; s < sections.length; ++s)
                if (sections[s].list.length) { selection = start(s); break }
        } else if (key === Qt.Key_Up) {
            if (!section.row && p.offset > 0) --selection
            else for (let s = p.section - 1; s >= 0; --s)
                if (sections[s].list.length) { selection = start(s); break }
        }
        reveal()
    }
    function reveal() {
        const p = place(selection)
        if (p.section < 0) return
        let item
        if (p.section === 2) {
            item = recentRepeater.itemAt(p.offset)
        } else {
            item = p.section === 0 ? playlistRow : continueRow
            item.positionViewAtIndex(p.offset, ListView.Contain)
        }
        if (!item) return
        // Rows bring their section title along.
        const above = p.section === 2 ? 12 : 44
        const y = item.mapToItem(scroller.contentItem, 0, 0).y
        if (y - above < scroller.contentY) scroller.contentY = Math.max(0, y - above)
        else if (y + item.height > scroller.contentY + scroller.height) scroller.contentY = y + item.height - scroller.height + 12
    }
    onTotalChanged: if (selection >= total) selection = total - 1

    Keys.onPressed: (event) => {
        if (app.helpVisible || editingId) return
        if ([Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down].includes(event.key)) {
            move(event.key); event.accepted = true
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) && selection >= 0) {
            event.accepted = true
            if (!event.isAutoRepeat) open(selection)
        } else if (event.key === Qt.Key_Delete && selection >= 0) {
            // One entry per press: a held key must not clear the history.
            event.accepted = true
            if (event.isAutoRepeat) return
            remove(selection)
        }
    }

    Rectangle { anchors.fill: parent; color: theme.background }

    // Header.
    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 76

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            Rectangle {
                width: 32; height: 32
                radius: Math.min(theme.radius, 9)
                color: theme.accent
                anchors.verticalCenter: parent.verticalCenter
                Glyph { anchors.centerIn: parent; anchors.horizontalCenterOffset: 1; width: 16; height: 16; name: "play"; color: theme.accentText }
            }
            Text {
                text: "mpv-extended"
                color: theme.strong
                font.pixelSize: 20
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            // Narrow windows keep the name readable: the secondary buttons
            // shrink to icons, with their names in the tooltip.
            PillButton { icon: "link"; text: qsTr("Paste link"); hint: KeyNames.label("Ctrl+V"); compact: home.width < 760; onClicked: home.app.openClipboard() }
            PillButton { icon: "folder"; text: qsTr("Open folder"); hint: KeyNames.label("Ctrl+Shift+O"); compact: home.width < 760; onClicked: picker.openFolder() }
            PillButton { icon: "plus"; text: qsTr("Open"); hint: KeyNames.label("Ctrl+O"); primary: true; onClicked: picker.openFiles() }
            IconButton { icon: "help"; tip: qsTr("Shortcuts (?)"); anchors.verticalCenter: parent.verticalCenter; onClicked: home.app.helpVisible = !home.app.helpVisible }
        }
    }

    // Empty state: one obvious thing to do.
    Item {
        visible: home.empty
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        Rectangle {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: parent.height > 420 ? -30 : 0
            width: Math.min(parent.width - 64, 560)
            // Shorter in short windows, so it never reaches the header.
            height: Math.min(300, parent.height - 32)
            radius: theme.radius
            color: home.app.dropActive ? theme.alpha(theme.accent, 0.08) : theme.alpha(theme.surface, 0.5)
            border.width: 1
            border.color: home.app.dropActive ? theme.accent : theme.alpha(theme.foreground, 0.12)
            Column {
                anchors.centerIn: parent
                spacing: 14
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 72; height: 72
                    radius: Math.min(theme.radius, 20)
                    color: theme.alpha(theme.accent, 0.14)
                    Glyph { anchors.centerIn: parent; width: 34; height: 34; name: "film"; color: theme.accent; lineWidth: 1.5 }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Drop a video or a folder here")
                    color: theme.strong
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("or press Ctrl+O to pick one. Links work too: Ctrl+V.")
                    color: theme.dim
                    font.pixelSize: 13
                }
            }
        }
    }

    Flickable {
        id: scroller
        visible: !home.empty
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentHeight: content.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Behavior on contentY { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        Column {
            id: content
            width: scroller.width
            spacing: 14
            topPadding: 8

            SectionTitle { text: qsTr("Playlists") }
            ListView {
                id: playlistRow
                width: parent.width
                height: 246
                orientation: ListView.Horizontal
                spacing: 18
                leftMargin: 28
                rightMargin: 28
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: home.playlistList
                delegate: PlaylistCard {
                    required property var modelData
                    required property int index
                    entry: modelData
                    selected: home.selection === index
                    y: 4
                    onActivated: home.open(index)
                    onEditRequested: home.editPlaylist(modelData.id)
                    onRemoveRequested: playlists.forget(modelData.id)
                }
                // Always last in the row: a new, empty playlist to fill.
                footer: Item {
                    width: newTile.width + (home.playlistList.length ? 18 : 0)
                    height: newTile.height + 4
                    Rectangle {
                        id: newTile
                        x: home.playlistList.length ? 18 : 0
                        y: 10
                        width: 180
                        height: Math.round(296 * 9 / 16)
                        radius: theme.radius
                        color: newArea.containsMouse ? theme.alpha(theme.accent, 0.08) : "transparent"
                        border.color: newArea.containsMouse ? theme.accent : theme.alpha(theme.foreground, 0.15)
                        Column {
                            anchors.centerIn: parent
                            spacing: 8
                            Glyph { anchors.horizontalCenter: parent.horizontalCenter; width: 28; height: 28; name: "plus"; color: theme.accent }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("New playlist"); color: theme.strong; font.pixelSize: 13; font.weight: Font.DemiBold }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: KeyNames.label("Ctrl+N"); color: theme.dim; font.pixelSize: 11 }
                        }
                        MouseArea { id: newArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: home.newPlaylist() }
                    }
                }
            }

            SectionTitle { visible: home.continueList.length > 0; text: qsTr("Continue watching"); topPadding: 10 }
            ListView {
                id: continueRow
                visible: home.continueList.length > 0
                width: parent.width
                height: 240
                orientation: ListView.Horizontal
                spacing: 18
                leftMargin: 28
                rightMargin: 28
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: home.continueList
                delegate: MediaCard {
                    required property var modelData
                    required property int index
                    entry: modelData
                    selected: home.selection === home.playlistList.length + index
                    y: 4
                    onActivated: home.open(home.playlistList.length + index)
                    onRemoveRequested: library.forget(modelData.path)
                }
            }

            SectionTitle { visible: home.recentList.length > 0; text: qsTr("Recently played"); topPadding: 10 }
            Column {
                width: parent.width
                Repeater {
                    id: recentRepeater
                    model: home.recentList
                    RecentRow {
                        required property var modelData
                        required property int index
                        entry: modelData
                        width: content.width
                        selected: home.selection === home.playlistList.length + home.continueList.length + index
                        onActivated: home.open(home.playlistList.length + home.continueList.length + index)
                        onRemoveRequested: library.forget(modelData.path)
                    }
                }
            }
        }
    }

    PlaylistEditor {
        id: editor
        anchors.fill: parent
        app: home.app
        playlistId: home.editingId
        visible: home.editingId !== ""
        focus: visible
        onClosed: home.closeEditor()
    }

    component SectionTitle: Text {
        leftPadding: 28
        color: theme.strong
        font.pixelSize: 15
        font.weight: Font.DemiBold
    }
}
