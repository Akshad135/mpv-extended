import QtQuick

// A continue-watching card: the frame where playback stopped, how far along
// it is, and how much is left.
Item {
    id: card
    required property var entry
    property bool selected: false
    signal activated()
    signal removeRequested()

    width: 296
    height: thumb.height + 62
    scale: area.pressed ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

    Rectangle {
        id: thumb
        width: parent.width
        height: Math.round(width * 9 / 16)
        radius: theme.radius
        color: theme.surface
        clip: true
        border.width: card.selected || area.containsMouse ? 2 : 1
        border.color: card.selected ? theme.accent : area.containsMouse ? theme.alpha(theme.accent, 0.7) : theme.alpha(theme.foreground, 0.08)

        Glyph {
            anchors.centerIn: parent
            visible: image.status !== Image.Ready
            width: 40; height: 40
            name: card.entry.kind === "audio" ? "music" : "film"
            color: theme.muted
            lineWidth: 1.4
        }
        Image {
            id: image
            anchors.fill: parent
            anchors.margins: thumb.border.width
            source: card.entry.thumbnail
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
            sourceSize.width: 600
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
        // Darkens the bottom so the time chip and bar read on bright frames.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: thumb.border.width
            height: 48
            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: theme.alpha(theme.deep, 0.7) }
            }
        }
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: progressTrack.top
            anchors.margins: 8
            width: remaining.implicitWidth + 12
            height: 20
            radius: Math.min(4, theme.radius)
            color: theme.alpha(theme.deep, 0.8)
            Text {
                id: remaining
                anchors.centerIn: parent
                text: qsTr("%1 left").arg(library.formatTime(card.entry.remaining))
                color: theme.strong
                font.pixelSize: 11
                font.family: "monospace"
            }
        }
        Rectangle {
            id: progressTrack
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: thumb.border.width
            height: 4
            color: theme.alpha(theme.foreground, 0.2)
            Rectangle {
                width: parent.width * card.entry.progress
                height: parent.height
                color: theme.accent
            }
        }
        // Play hint on hover.
        Rectangle {
            anchors.centerIn: parent
            width: 52; height: 52; radius: 26
            color: theme.accent
            opacity: area.containsMouse || card.selected ? 1 : 0
            scale: area.containsMouse || card.selected ? 1 : 0.8
            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
            Glyph { anchors.centerIn: parent; anchors.horizontalCenterOffset: 2; width: 22; height: 22; name: "play"; color: theme.accentText }
        }
    }

    Column {
        anchors.top: thumb.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 3
        Text {
            width: parent.width
            text: card.entry.title
            color: card.entry.available ? theme.strong : theme.dim
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: card.entry.available ? [card.entry.folder, library.relativeTime(card.entry.lastPlayed)].filter(s => s).join("  ·  ")
                                       : qsTr("File is missing")
            color: card.entry.available ? theme.dim : theme.red
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.activated()
    }

    IconButton {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        size: 28
        icon: "close"
        tip: qsTr("Remove from history (Delete)")
        visible: area.containsMouse || hovered
        property bool hovered: false
        HoverHandler { onHoveredChanged: parent.hovered = hovered }
        onClicked: card.removeRequested()
    }
}
