import QtQuick
import QtQuick.Controls

// Every shortcut, on ? .
Rectangle {
    id: help
    property bool shown: false
    signal dismissed()

    color: theme.alpha(theme.deep, 0.82)
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 160 } }

    readonly property var groups: [
        { title: qsTr("Playback"), keys: [
            ["Space", qsTr("Play or pause")],
            ["← →", qsTr("Back or forward 5 s")],
            ["J  L", qsTr("Back or forward 10 s")],
            ["Shift+← →", qsTr("Back or forward 1 s")],
            [", .", qsTr("Previous or next frame")],
            ["0 – 9", qsTr("Jump to 0–90%")],
            ["PgUp PgDn", qsTr("Previous or next chapter")],
            ["[ ]", qsTr("Slower or faster")],
            ["Backspace", qsTr("Normal speed")],
        ]},
        { title: qsTr("Sound and subtitles"), keys: [
            ["↑ ↓", qsTr("Volume")],
            ["M", qsTr("Mute")],
            ["A", qsTr("Next audio track")],
            ["S", qsTr("Next subtitle track")],
            ["T", qsTr("Audio and subtitle menu")],
        ]},
        { title: qsTr("Window and queue"), keys: [
            ["F", qsTr("Fullscreen")],
            ["N  Shift+N", qsTr("Next or previous file")],
            ["P", qsTr("Show the queue")],
            ["Ctrl+S", qsTr("Save the frame to Pictures")],
            ["Esc", qsTr("Close, leave fullscreen, or go to the library")],
        ]},
        { title: qsTr("Library"), keys: [
            ["Ctrl+O", qsTr("Open files")],
            ["Ctrl+Shift+O", qsTr("Open a folder")],
            ["Ctrl+V", qsTr("Open a copied link or path")],
            ["Arrows  Enter", qsTr("Pick and play")],
            ["Delete", qsTr("Remove from history")],
            ["?", qsTr("Show these shortcuts")],
            ["Q", qsTr("Quit")],
        ]},
    ]

    MouseArea { anchors.fill: parent; onClicked: help.dismissed() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 860)
        height: Math.min(parent.height - 48, grid.implicitHeight + 88)
        radius: theme.radius
        color: theme.background
        border.color: theme.alpha(theme.foreground, 0.12)
        clip: true
        MouseArea { anchors.fill: parent }

        Text {
            id: heading
            x: 28; y: 22
            text: qsTr("Keyboard shortcuts")
            color: theme.strong
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }
        // Scrolls when the window is too small to show every group.
        Flickable {
            id: scroller
            anchors.top: heading.bottom
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            anchors.bottomMargin: 12
            contentHeight: grid.implicitHeight + 12
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Flow {
            id: grid
            width: scroller.width
            spacing: 28
            Repeater {
                model: help.groups
                Column {
                    required property var modelData
                    width: grid.width >= 560 ? (grid.width - 28) / 2 : grid.width
                    spacing: 6
                    Text {
                        text: modelData.title
                        color: theme.accent
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 0.8
                        bottomPadding: 2
                    }
                    Repeater {
                        model: modelData.keys
                        Item {
                            required property var modelData
                            width: parent.width
                            height: 24
                            Rectangle {
                                id: cap
                                height: 22
                                width: Math.max(26, keyText.implicitWidth + 14)
                                radius: Math.min(4, theme.radius)
                                color: theme.surface
                                border.color: theme.alpha(theme.foreground, 0.12)
                                Text {
                                    id: keyText
                                    anchors.centerIn: parent
                                    text: modelData[0]
                                    color: theme.strong
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                }
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 130
                                anchors.right: parent.right
                                anchors.verticalCenter: cap.verticalCenter
                                text: modelData[1]
                                color: theme.foreground
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
        }
    }
}
