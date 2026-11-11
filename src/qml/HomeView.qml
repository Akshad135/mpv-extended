import QtQuick
import QtQuick.Controls
import "Keys.js" as KeyNames

// The library: pick up where you left off, or open something new.
FocusScope {
    id: home
    required property var app
    readonly property var continueList: library.continueWatching
    readonly property var recentList: library.recent
    readonly property int total: continueList.length + recentList.length
    readonly property bool empty: total === 0
    property int selection: -1

    function entryAt(i) { return i < continueList.length ? continueList[i] : recentList[i - continueList.length] }
    function open(entry) { if (entry) app.openInputs([entry.path]) }
    function move(key) {
        if (total === 0) return
        const n1 = continueList.length
        if (selection < 0) { selection = 0; return }
        const inRow = selection < n1
        if (key === Qt.Key_Left && inRow) selection = Math.max(0, selection - 1)
        else if (key === Qt.Key_Right && inRow) selection = Math.min(n1 - 1, selection + 1)
        else if (key === Qt.Key_Down) selection = inRow ? (recentList.length ? n1 : selection) : Math.min(total - 1, selection + 1)
        else if (key === Qt.Key_Up) selection = inRow ? selection : (selection === n1 && n1 > 0 ? 0 : Math.max(0, selection - 1))
        reveal()
    }
    function reveal() {
        if (selection < continueList.length) {
            continueRow.positionViewAtIndex(selection, ListView.Contain)
            scroller.contentY = 0
        } else {
            const item = recentRepeater.itemAt(selection - continueList.length)
            if (!item) return
            const y = item.mapToItem(scroller.contentItem, 0, 0).y
            if (y < scroller.contentY) scroller.contentY = y - 12
            else if (y + item.height > scroller.contentY + scroller.height) scroller.contentY = y + item.height - scroller.height + 12
        }
    }
    onTotalChanged: if (selection >= total) selection = total - 1

    Keys.onPressed: (event) => {
        if (app.helpVisible) return
        if ([Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down].includes(event.key)) {
            move(event.key); event.accepted = true
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) && selection >= 0) {
            open(entryAt(selection)); event.accepted = true
        } else if (event.key === Qt.Key_Delete && selection >= 0) {
            library.forget(entryAt(selection).path); event.accepted = true
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

            SectionTitle { visible: home.continueList.length > 0; text: qsTr("Continue watching") }
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
                    selected: home.selection === index
                    y: 4
                    onActivated: home.open(modelData)
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
                        selected: home.selection === home.continueList.length + index
                        onActivated: home.open(modelData)
                        onRemoveRequested: library.forget(modelData.path)
                    }
                }
            }
        }
    }

    component SectionTitle: Text {
        leftPadding: 28
        color: theme.strong
        font.pixelSize: 15
        font.weight: Font.DemiBold
    }

    component PillButton: Rectangle {
        id: pill
        property string icon
        property string text
        property string hint
        property bool primary: false
        property bool compact: false
        signal clicked()
        height: 36
        width: compact ? height : pillRow.implicitWidth + 28
        radius: Math.min(theme.radius, 18)
        color: primary ? (pillArea.containsMouse ? theme.mix(theme.accent, theme.strong, 0.15) : theme.accent)
                       : (pillArea.containsMouse ? theme.alpha(theme.foreground, 0.1) : theme.alpha(theme.foreground, 0.05))
        border.color: primary ? "transparent" : theme.alpha(theme.foreground, 0.12)
        Behavior on color { ColorAnimation { duration: 120 } }
        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 8
            Glyph { name: pill.icon; width: 16; height: 16; color: pill.primary ? theme.accentText : theme.strong; anchors.verticalCenter: parent.verticalCenter }
            Text { visible: !pill.compact; text: pill.text; color: pill.primary ? theme.accentText : theme.strong; font.pixelSize: 13; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea { id: pillArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: pill.clicked() }
        ToolTip.visible: hint.length > 0 && pillArea.containsMouse
        ToolTip.text: compact ? text + "  " + hint : hint
        ToolTip.delay: 600
    }
}
