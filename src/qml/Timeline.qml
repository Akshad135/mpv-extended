import QtQuick

// The seek bar: played, buffered, chapter breaks, and a frame preview that
// follows the pointer. Dragging scrubs on keyframes and lands exactly.
Item {
    id: timeline
    property real position: 0
    property real duration: 0
    property real cacheEnd: 0
    property var chapters: []
    readonly property bool active: area.containsMouse || area.pressed
    readonly property bool scrubbing: area.pressed
    property real scrubValue: 0
    signal seekRequested(real seconds, bool exact)

    height: 22
    readonly property real shown: scrubbing ? scrubValue : position
    readonly property real fraction: duration > 0 ? Math.max(0, Math.min(1, shown / duration)) : 0
    readonly property real hoverFraction: Math.max(0, Math.min(1, area.mouseX / width))

    function chapterAt(seconds) {
        let title = ""
        for (let i = 0; i < chapters.length; ++i)
            if (chapters[i].time <= seconds) title = chapters[i].title || ""
        return title
    }

    Item {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: timeline.active ? 6 : 4
        Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: theme.alpha(theme.foreground, 0.18)
        }
        Rectangle {
            height: parent.height
            radius: height / 2
            width: timeline.duration > 0 ? parent.width * Math.min(1, timeline.cacheEnd / timeline.duration) : 0
            color: theme.alpha(theme.foreground, 0.22)
        }
        Rectangle {
            height: parent.height
            radius: height / 2
            width: parent.width * timeline.fraction
            color: theme.accent
        }
        // Hover ghost: how far a click would go.
        Rectangle {
            visible: area.containsMouse && !area.pressed
            height: parent.height
            radius: height / 2
            x: Math.min(area.mouseX, parent.width * timeline.fraction)
            width: Math.abs(area.mouseX - parent.width * timeline.fraction)
            color: theme.alpha(theme.accent, 0.3)
        }
        // Chapter breaks are cut into the bar as small gaps.
        Repeater {
            model: timeline.duration > 0 ? timeline.chapters : []
            Rectangle {
                required property var modelData
                visible: modelData.time > 0.5
                x: track.width * modelData.time / timeline.duration - 1
                width: 2
                height: track.height
                color: theme.deep
            }
        }
    }

    Rectangle {
        id: handle
        width: 14
        height: 14
        radius: 7
        color: theme.accent
        border.color: theme.alpha(theme.accentText, 0.25)
        border.width: 1
        x: timeline.width * timeline.fraction - width / 2
        anchors.verticalCenter: parent.verticalCenter
        scale: timeline.active ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }
    }

    // Preview card above the pointer.
    Rectangle {
        id: preview
        readonly property real at: timeline.scrubbing ? timeline.scrubValue : timeline.hoverFraction * timeline.duration
        readonly property string frame: thumbnailer.revision >= 0 && timeline.duration > 0
                                        ? thumbnailer.frameAt(at / timeline.duration) : ""
        readonly property string chapter: timeline.chapterAt(at)
        visible: timeline.active && timeline.duration > 0
        width: frame ? 192 : Math.max(64, label.implicitWidth + 20)
        height: (frame ? 108 + 6 : 0) + (chapter ? 46 : 30)
        x: Math.max(0, Math.min(timeline.width - width,
                                (timeline.scrubbing ? timeline.fraction * timeline.width : area.mouseX) - width / 2))
        y: -height - 14
        radius: theme.radius
        color: theme.alpha(theme.background, 0.96)
        border.color: theme.alpha(theme.foreground, 0.14)

        Image {
            id: frameImage
            visible: preview.frame.length > 0
            source: preview.frame
            x: 3; y: 3
            width: parent.width - 6
            height: 108
            fillMode: Image.PreserveAspectCrop
            asynchronous: false
            cache: true
            smooth: true
        }
        Column {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 7
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 16
            spacing: 1
            Text {
                visible: preview.chapter.length > 0
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: preview.chapter
                color: theme.foreground
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Text {
                id: label
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: library.formatTime(preview.at)
                color: theme.strong
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.family: "monospace"
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        anchors.topMargin: -6
        anchors.bottomMargin: -6
        hoverEnabled: true
        preventStealing: true
        cursorShape: Qt.PointingHandCursor
        function valueAt(x) { return Math.max(0, Math.min(1, x / width)) * timeline.duration }
        onPressed: (mouse) => {
            timeline.scrubValue = valueAt(mouse.x)
            timeline.seekRequested(timeline.scrubValue, false)
        }
        onPositionChanged: (mouse) => {
            if (!pressed) return
            timeline.scrubValue = valueAt(mouse.x)
            timeline.seekRequested(timeline.scrubValue, false)
        }
        onReleased: (mouse) => timeline.seekRequested(valueAt(mouse.x), true)
    }
}
