import QtQuick

// A history row: small frame, title, folder, and when it was played.
Item {
    id: row
    required property var entry
    property bool selected: false
    signal activated()
    signal removeRequested()

    height: 64

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        radius: Math.min(6, theme.radius)
        color: row.selected ? theme.alpha(theme.accent, 0.12)
             : area.containsMouse ? theme.alpha(theme.foreground, 0.05) : "transparent"
        border.color: row.selected ? theme.alpha(theme.accent, 0.5) : "transparent"
    }

    Rectangle {
        id: thumb
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.verticalCenter: parent.verticalCenter
        width: 80; height: 45
        radius: Math.min(4, theme.radius)
        color: theme.surface
        clip: true
        Glyph {
            anchors.centerIn: parent
            visible: image.status !== Image.Ready
            width: 20; height: 20
            name: row.entry.kind === "audio" ? "music" : "film"
            color: theme.muted
        }
        Image {
            id: image
            anchors.fill: parent
            source: row.entry.thumbnail
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
            sourceSize.width: 200
        }
    }

    Column {
        anchors.left: thumb.right
        anchors.leftMargin: 14
        anchors.right: meta.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        Text {
            width: parent.width
            text: row.entry.title
            color: row.entry.available ? theme.strong : theme.dim
            font.pixelSize: 14
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: row.entry.available ? row.entry.folder : qsTr("File is missing")
            color: row.entry.available ? theme.dim : theme.red
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    Row {
        id: meta
        anchors.right: parent.right
        anchors.rightMargin: 32
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14
        Glyph {
            visible: row.entry.finished
            name: "check"
            width: 16; height: 16
            color: theme.green
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: row.entry.duration > 0 ? library.formatTime(row.entry.duration) : ""
            color: theme.dim
            font.pixelSize: 12
            font.family: "monospace"
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            width: 90
            horizontalAlignment: Text.AlignRight
            text: library.relativeTime(row.entry.lastPlayed)
            color: theme.dim
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
        }
        Item {
            width: 28; height: 28
            anchors.verticalCenter: parent.verticalCenter
            IconButton {
                anchors.fill: parent
                size: 28
                icon: "close"
                tip: qsTr("Remove from history (Delete)")
                visible: area.containsMouse || closeHover.hovered
                HoverHandler { id: closeHover }
                onClicked: row.removeRequested()
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: row.activated()
        z: -1
    }
}
