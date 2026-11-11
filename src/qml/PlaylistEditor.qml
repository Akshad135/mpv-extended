import QtQuick
import QtQuick.Controls
import "Keys.js" as KeyNames

// One playlist: rename it, add and remove files, drag them into order, and
// play from any of them. Every change is saved as it's made.
FocusScope {
    id: editor
    required property var app
    property string playlistId
    signal closed()
    // The playlist got a new id (a folder's playlist became a list of its
    // own); whoever opened the page follows it.
    signal moved(string id)

    readonly property var info: { playlists.all; return playlists.find(playlistId) }
    property int selection: -1
    property bool dragging: false
    property string orderBeforeDrag
    property bool committing: false
    property bool confirmDelete: false

    ListModel { id: files }

    function paths() {
        const out = []
        for (let i = 0; i < files.count; ++i) out.push(files.get(i).path)
        return out
    }
    function reload() {
        const entries = playlists.entries(playlistId)
        files.clear()
        for (const entry of entries) files.append(entry)
        selection = Math.min(selection, files.count - 1)
    }
    // Saved after every change. Editing a folder's playlist makes it a list
    // of its own, with a new id.
    function commit() {
        committing = true
        const id = playlists.setFiles(playlistId, paths())
        committing = false
        if (!id) {
            app.showError(qsTr("Couldn't save the playlist."))
            reload()
            return
        }
        if (id !== playlistId) moved(id)
    }
    function add(urls) {
        const added = library.collect(urls)
        if (added.length === 0) {
            app.showError(qsTr("Nothing playable there."))
            return
        }
        const known = new Set(paths())
        let count = 0
        for (const path of added) {
            if (known.has(path)) continue
            known.add(path)
            ++count
            files.append({ path: path, title: path.includes("://") ? path : path.split("/").pop().replace(/\.[^.]+$/, ""),
                           folder: path.includes("://") ? "" : path.split("/").slice(-2, -1)[0] || "",
                           duration: 0, finished: false, available: true })
        }
        if (count > 0) commit()
        app.toast("playlist", count > 0 ? qsTr("%n added", "", count) : qsTr("Already in the playlist"))
    }
    function moveRow(from, to) {
        if (from === to || from < 0 || to < 0 || from >= files.count || to >= files.count) return
        files.move(from, to, 1)
        selection = to
        list.positionViewAtIndex(to, ListView.Contain)
        commit()
    }
    function removeRow(i) {
        if (i < 0 || i >= files.count) return
        files.remove(i)
        if (i < selection) --selection
        selection = Math.min(selection, files.count - 1)
        commit()
    }
    function play(path) {
        saveName()
        app.openPlaylist(playlistId, path || "")
    }
    function renameNow() { nameField.forceActiveFocus(); nameField.selectAll() }
    // Keys go to the list's handler, not the name. (Focusing the scope
    // itself would hand focus straight back to the name.)
    function takeKeys() { keySink.forceActiveFocus() }
    // A typed name is kept however the page is left: Enter, a click
    // elsewhere, Back, or another playlist opened.
    function saveName() {
        const name = nameField.text.trim()
        if (playlistId && name && name !== info.name) playlists.rename(playlistId, name)
        nameField.text = Qt.binding(() => editor.info.name || "")
    }
    // Esc leaves a rename unchanged first, and only then the page.
    function leave() {
        if (nameField.activeFocus) {
            nameField.text = Qt.binding(() => editor.info.name || "")
            takeKeys()
        } else {
            saveName()
            closed()
        }
    }
    Item { id: keySink; focus: true }

    onPlaylistIdChanged: if (paths().join("\n") !== playlists.entries(playlistId).map(e => e.path).join("\n")) reload()
    // Changes from another window, or a folder scanned again on opening.
    Connections {
        target: playlists
        function onChanged() {
            if (editor.dragging || editor.committing || !editor.playlistId) return
            if (!editor.info.id) { editor.closed(); return } // removed elsewhere
            if (editor.paths().join("\n") !== playlists.entries(editor.playlistId).map(e => e.path).join("\n"))
                editor.reload()
        }
    }

    Keys.onPressed: (event) => {
        if (nameField.activeFocus) return // keys typed into the name stay there
        const ctrl = event.modifiers & (Qt.ControlModifier | Qt.AltModifier)
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            event.accepted = true
            if (files.count === 0) return
            const step = event.key === Qt.Key_Up ? -1 : 1
            if (selection < 0) { selection = 0; return }
            if (ctrl) moveRow(selection, selection + step)
            else {
                selection = Math.max(0, Math.min(files.count - 1, selection + step))
                list.positionViewAtIndex(selection, ListView.Contain)
            }
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && selection >= 0) {
            event.accepted = true
            if (!event.isAutoRepeat) play(files.get(selection).path)
        } else if (event.key === Qt.Key_Delete && selection >= 0) {
            event.accepted = true
            if (!event.isAutoRepeat) removeRow(selection)
        }
    }

    Rectangle { anchors.fill: parent; color: theme.background }
    // Clicks between the controls stay on this page, not the library under it.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: editor.takeKeys(); onWheel: (wheel) => wheel.accepted = true }

    // Header: back, the name (edited in place), and what can be done.
    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 112

        IconButton {
            id: back
            x: 16
            y: 20
            icon: "back"
            tip: qsTr("Library (Esc)")
            onClicked: editor.closed()
        }
        // The name, edited in place: looks like a title until clicked.
        Rectangle {
            id: nameBox
            anchors.left: back.right
            anchors.leftMargin: 8
            anchors.right: actions.left
            anchors.rightMargin: 16
            anchors.verticalCenter: back.verticalCenter
            height: 40
            radius: Math.min(6, theme.radius)
            color: nameField.activeFocus ? theme.alpha(theme.foreground, 0.05) : "transparent"
            border.color: nameField.activeFocus ? theme.accent : nameHover.hovered ? theme.alpha(theme.foreground, 0.15) : "transparent"
            HoverHandler { id: nameHover; cursorShape: Qt.IBeamCursor }
            TextInput {
                id: nameField
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                text: editor.info.name || ""
                font.pixelSize: 20
                font.weight: Font.Bold
                color: theme.strong
                selectionColor: theme.alpha(theme.accent, 0.4)
                selectedTextColor: theme.strong
                selectByMouse: true
                // The start of a long name shows when it's not being edited.
                autoScroll: activeFocus
                onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
                onAccepted: editor.takeKeys()
                onEditingFinished: editor.saveName()
            }
        }
        Text {
            anchors.left: nameBox.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: nameBox.bottom
            anchors.topMargin: 6
            text: {
                const parts = [qsTr("%n files", "", files.count)]
                if (editor.info.watched > 0) parts.push(qsTr("%1 watched").arg(editor.info.watched))
                if (editor.info.kind === "folder") parts.push(qsTr("Follows its folder; editing makes it a list of its own"))
                else if (editor.info.kind === "file") parts.push(qsTr("Follows its playlist file; editing makes it a list of its own"))
                return parts.join("  ·  ")
            }
            color: theme.dim
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Row {
            id: actions
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: back.verticalCenter
            spacing: 8
            readonly property bool compact: editor.width < 820
            PillButton {
                icon: "trash"
                text: editor.confirmDelete ? qsTr("Delete playlist?") : qsTr("Delete")
                compact: actions.compact && !editor.confirmDelete
                hint: editor.confirmDelete ? qsTr("Click again to delete") : ""
                color: editor.confirmDelete ? theme.alpha(theme.red, 0.25) : theme.alpha(theme.foreground, 0.05)
                onClicked: {
                    if (!editor.confirmDelete) { editor.confirmDelete = true; confirmTimer.restart(); return }
                    playlists.forget(editor.playlistId)
                    editor.closed()
                }
                Timer { id: confirmTimer; interval: 3000; onTriggered: editor.confirmDelete = false }
            }
            PillButton { icon: "folder"; text: qsTr("Add folder"); compact: actions.compact; onClicked: picker.openFolder("playlist") }
            PillButton { icon: "plus"; text: qsTr("Add files"); compact: actions.compact; onClicked: picker.openFiles("playlist") }
            PillButton { icon: "play"; text: qsTr("Play"); primary: true; onClicked: editor.play("") }
        }
    }

    // Empty: one obvious thing to do.
    Column {
        visible: files.count === 0
        anchors.centerIn: list
        spacing: 10
        Glyph { anchors.horizontalCenter: parent.horizontalCenter; width: 40; height: 40; name: "playlist"; color: theme.muted; lineWidth: 1.4 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("No files yet"); color: theme.strong; font.pixelSize: 16; font.weight: Font.DemiBold }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Add files or a folder, or drop them here."); color: theme.dim; font.pixelSize: 13 }
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        clip: true
        model: files
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        // Rows slide aside while one is dragged past them.
        displaced: Transition { NumberAnimation { properties: "y"; duration: 160; easing.type: Easing.OutCubic } }

        // Scrolls while a row is held near the top or bottom edge.
        Timer {
            id: edgeScroll
            property real speed: 0
            interval: 16
            repeat: true
            running: editor.dragging && speed !== 0
            onTriggered: list.contentY = Math.max(list.originY,
                                                  Math.min(list.contentY + speed, list.originY + list.contentHeight - list.height))
        }

        delegate: DropArea {
            id: slot
            required property int index
            required property string path
            required property string title
            required property string folder
            required property real duration
            required property bool finished
            required property bool available
            width: ListView.view.width
            height: 54
            // Another row dragged over this one takes its place.
            onEntered: (drag) => {
                const from = drag.source.rowIndex
                if (from !== slot.index) files.move(from, slot.index, 1)
            }

            Item {
                id: row
                property int rowIndex: slot.index
                width: slot.width
                height: slot.height
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                Drag.active: grip.drag.active
                Drag.source: row
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                states: State {
                    when: grip.drag.active
                    // Floats above the other rows while held.
                    ParentChange { target: row; parent: editor }
                    AnchorChanges { target: row; anchors.horizontalCenter: undefined; anchors.verticalCenter: undefined }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    radius: Math.min(6, theme.radius)
                    color: grip.drag.active ? theme.surface
                         : editor.selection === slot.index ? theme.alpha(theme.accent, 0.12)
                         : rowArea.containsMouse ? theme.alpha(theme.foreground, 0.05) : "transparent"
                    border.color: grip.drag.active ? theme.accent
                                : editor.selection === slot.index ? theme.alpha(theme.accent, 0.5) : "transparent"
                }

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { editor.selection = slot.index; editor.takeKeys() }
                    onDoubleClicked: editor.play(slot.path)
                }

                // The grip: press and drag to move the row.
                MouseArea {
                    id: grip
                    objectName: "grip" + slot.index
                    x: 22
                    width: 28
                    height: parent.height
                    cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    drag.target: row
                    drag.axis: Drag.YAxis
                    onPressed: {
                        editor.selection = slot.index
                        editor.takeKeys()
                        editor.orderBeforeDrag = editor.paths().join("\n")
                        editor.dragging = true
                    }
                    onPositionChanged: {
                        if (!drag.active) return
                        const y = row.mapToItem(list, 0, 0).y
                        edgeScroll.speed = y < 24 ? -10 : y + row.height > list.height - 24 ? 10 : 0
                    }
                    function finish() {
                        edgeScroll.speed = 0
                        editor.dragging = false
                        editor.selection = row.rowIndex
                        // A click on the grip changes nothing, so saves nothing
                        // (and leaves a folder's playlist following its folder).
                        if (editor.paths().join("\n") !== editor.orderBeforeDrag) editor.commit()
                    }
                    onReleased: finish()
                    onCanceled: finish()
                    Glyph { anchors.centerIn: parent; width: 18; height: 18; name: "grip"; lineWidth: 3.2; color: theme.muted }
                }

                // The number, or a play button on hover.
                Item {
                    id: number
                    x: 54
                    width: 32
                    height: parent.height
                    Text {
                        anchors.centerIn: parent
                        visible: !rowArea.containsMouse && !playHere.containsMouse
                        text: String(slot.index + 1)
                        color: theme.dim
                        font.pixelSize: 12
                        font.family: "monospace"
                    }
                    Glyph {
                        anchors.centerIn: parent
                        visible: rowArea.containsMouse || playHere.containsMouse
                        width: 16; height: 16
                        name: "play"
                        color: theme.accent
                    }
                    MouseArea {
                        id: playHere
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: editor.play(slot.path)
                    }
                    ToolTip.visible: playHere.containsMouse
                    ToolTip.text: qsTr("Play from here")
                    ToolTip.delay: 600
                }

                Column {
                    anchors.left: number.right
                    anchors.leftMargin: 10
                    anchors.right: meta.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        width: parent.width
                        text: slot.title
                        color: slot.available ? theme.strong : theme.dim
                        font.pixelSize: 14
                        elide: Text.ElideMiddle
                    }
                    Text {
                        width: parent.width
                        text: slot.available ? slot.folder : qsTr("File is missing")
                        color: slot.available ? theme.dim : theme.red
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                Row {
                    id: meta
                    anchors.right: remove.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10
                    Glyph { visible: slot.finished; width: 14; height: 14; name: "check"; color: theme.green; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        visible: slot.duration > 0
                        text: library.formatTime(slot.duration)
                        color: theme.dim
                        font.pixelSize: 12
                        font.family: "monospace"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                IconButton {
                    id: remove
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    size: 28
                    icon: "close"
                    tip: qsTr("Remove from the playlist (%1)").arg(KeyNames.label("Delete"))
                    opacity: rowArea.containsMouse || hovered || editor.selection === slot.index ? 1 : 0
                    property bool hovered: false
                    HoverHandler { onHoveredChanged: parent.hovered = hovered }
                    onClicked: editor.removeRow(slot.index)
                }
            }
        }
    }
}
