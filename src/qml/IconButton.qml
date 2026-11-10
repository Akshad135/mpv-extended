import QtQuick
import QtQuick.Controls

// A borderless icon button that lights up under the pointer.
Item {
    id: button
    property string icon
    property string tip
    property bool active: false
    property bool enabledLook: true
    property real size: 36
    property real glyphSize: size * 0.56
    property color color: active ? theme.accent : theme.strong
    signal clicked()

    width: size
    height: size
    opacity: enabledLook ? 1 : 0.35

    Rectangle {
        anchors.fill: parent
        radius: Math.min(theme.radius, height / 2)
        color: area.pressed ? theme.alpha(theme.foreground, 0.16)
             : area.containsMouse ? theme.alpha(theme.foreground, 0.09) : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Glyph {
        anchors.centerIn: parent
        width: button.glyphSize
        height: button.glyphSize
        name: button.icon
        color: button.color
        scale: area.pressed ? 0.88 : 1
        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: if (button.enabledLook) button.clicked()
    }

    ToolTip.visible: tip.length > 0 && area.containsMouse
    ToolTip.text: tip
    ToolTip.delay: 600
}
