import QtQuick
import QtQuick.Controls

// A floating panel in the theme's colors. Escape is handled by the window so
// one press closes the topmost thing only.
Popup {
    id: sheet
    padding: 6
    closePolicy: Popup.CloseOnPressOutside
    transformOrigin: Popup.Bottom

    background: Rectangle {
        color: theme.alpha(theme.background, 0.96)
        border.color: theme.alpha(theme.foreground, 0.12)
        border.width: 1
        radius: theme.radius
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 100 }
    }
}
