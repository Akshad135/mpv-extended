import QtQuick
import QtQuick.Controls

// A rounded button with an icon and a label; primary ones are filled with
// the accent. Compact ones show only the icon, with the label in the tooltip.
Rectangle {
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
