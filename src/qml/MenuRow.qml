import QtQuick

// One choice in a Sheet: a check for the current one, a label, and a quiet
// detail on the right.
Rectangle {
    id: row
    property string text
    property string detail
    property bool checked: false
    signal clicked()

    width: parent ? parent.width : 220
    height: 34
    radius: Math.min(4, theme.radius)
    color: area.containsMouse ? theme.alpha(theme.foreground, 0.08) : "transparent"

    Glyph {
        id: mark
        name: "check"
        width: 16; height: 16
        color: theme.accent
        visible: row.checked
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
    }
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 32
        anchors.right: detailText.left
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: row.text
        color: row.checked ? theme.strong : theme.foreground
        font.pixelSize: 13
        font.weight: row.checked ? Font.DemiBold : Font.Normal
        elide: Text.ElideRight
    }
    Text {
        id: detailText
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: row.detail
        color: theme.dim
        font.pixelSize: 11
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: row.clicked()
    }
}
