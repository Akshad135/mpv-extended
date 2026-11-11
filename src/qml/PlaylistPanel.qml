import QtQuick
import QtQuick.Controls

// The queue, sliding in from the side. The current file is marked and kept
// in view.
Rectangle {
    id: panel
    required property var player
    property bool open: false
    signal picked(int index)
    signal closeRequested()

    radius: theme.radius
    color: theme.alpha(theme.background, 0.95)
    border.color: theme.alpha(theme.foreground, 0.12)
    visible: x < (parent ? parent.width : 0)

    // Clicks and the wheel on the panel stay here, not on the picture
    // (where the wheel sets the volume), even when the list can't scroll.
    MouseArea { anchors.fill: parent; onWheel: (wheel) => wheel.accepted = true }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52
        Column {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("Queue"); color: theme.strong; font.pixelSize: 15; font.weight: Font.DemiBold }
            Text {
                text: qsTr("%n items", "", panel.player.playlist.length)
                color: theme.dim
                font.pixelSize: 11
            }
        }
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            icon: "close"
            size: 32
            onClicked: panel.closeRequested()
        }
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        clip: true
        model: panel.player.playlist
        currentIndex: panel.player.playlistPos
        highlightFollowsCurrentItem: false
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        onCountChanged: positionViewAtIndex(Math.max(0, currentIndex), ListView.Center)
        // mpv resends the whole playlist on every file change, which resets
        // the view; bring the current file back into sight each time.
        function follow() { Qt.callLater(() => list.positionViewAtIndex(Math.max(0, panel.player.playlistPos), ListView.Contain)) }
        Connections {
            target: panel
            function onOpenChanged() { if (panel.open) list.follow() }
        }
        Connections {
            target: panel.player
            function onPlaylistChanged() { list.follow() }
            function onPlaylistPosChanged() { list.follow() }
        }

        delegate: Item {
            id: row
            required property var modelData
            required property int index
            readonly property bool current: index === panel.player.playlistPos
            readonly property string name: {
                const file = String(modelData.filename || "")
                return modelData.title || file.split("/").pop().replace(/\.[^.]+$/, "") || file
            }
            width: ListView.view.width
            height: 46

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Math.min(4, theme.radius)
                color: row.current ? theme.alpha(theme.accent, 0.14)
                     : rowArea.containsMouse ? theme.alpha(theme.foreground, 0.07) : "transparent"
            }
            Rectangle {
                visible: row.current
                width: 3
                height: parent.height - 16
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                radius: 1.5
                color: theme.accent
            }
            Text {
                id: number
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                text: row.current && !panel.player.paused ? "" : String(row.index + 1)
                color: row.current ? theme.accent : theme.dim
                font.pixelSize: 12
                font.family: "monospace"
            }
            Glyph {
                visible: row.current && !panel.player.paused
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 16; height: 16
                name: "audio"
                color: theme.accent
            }
            Text {
                anchors.left: number.right
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: row.name
                color: row.current ? theme.strong : theme.foreground
                font.pixelSize: 13
                font.weight: row.current ? Font.DemiBold : Font.Normal
                elide: Text.ElideMiddle
            }
            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: panel.picked(row.index)
            }
        }
    }
}
