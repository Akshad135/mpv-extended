import QtQuick

// A short note at the top: volume, track changes, saved frames, and errors.
// Errors stay longer and are tinted.
Rectangle {
    id: toast
    property string icon
    property string text
    property bool error: false

    function show(iconName, message, isError) {
        icon = iconName
        text = message
        error = isError === true
        hideTimer.interval = error ? 6000 : 1400
        opacity = 1
        hideTimer.restart()
    }

    width: Math.min(row.implicitWidth + 32, parent ? parent.width - 48 : 600)
    height: 40
    radius: Math.min(theme.radius, 20)
    color: error ? theme.mix(theme.background, theme.red, 0.18) : theme.alpha(theme.background, 0.94)
    border.color: error ? theme.alpha(theme.red, 0.6) : theme.alpha(theme.foreground, 0.12)
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 180 } }

    Timer { id: hideTimer; onTriggered: toast.opacity = 0 }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10
        Glyph {
            name: toast.error ? "warning" : toast.icon
            visible: name.length > 0
            width: 18; height: 18
            color: toast.error ? theme.red : theme.accent
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: toast.text
            color: theme.strong
            font.pixelSize: 13
            width: Math.min(implicitWidth, toast.parent ? toast.parent.width - 120 : 500)
            elide: Text.ElideRight
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
