import QtQuick
import QtQuick.Shapes

// Line icons drawn on a 24-unit grid, colored from the theme like text.
Item {
    id: glyph
    property string name
    property color color: theme.foreground
    property real lineWidth: 1.8
    implicitWidth: 20
    implicitHeight: 20

    readonly property var paths: ({
        "play": "M7.5 5.2v13.6a1 1 0 0 0 1.52.85l11-6.8a1 1 0 0 0 0-1.7l-11-6.8a1 1 0 0 0-1.52.85z",
        "pause": "M7 4.5h2.6a1 1 0 0 1 1 1v13a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1v-13a1 1 0 0 1 1-1zM14.4 4.5H17a1 1 0 0 1 1 1v13a1 1 0 0 1-1 1h-2.6a1 1 0 0 1-1-1v-13a1 1 0 0 1 1-1z",
        "next": "M5.5 6.1v11.8a.9.9 0 0 0 1.4.75l8.6-5.9a.9.9 0 0 0 0-1.5L6.9 5.35a.9.9 0 0 0-1.4.75zM17.2 5.5h1.6a.7.7 0 0 1 .7.7v11.6a.7.7 0 0 1-.7.7h-1.6a.7.7 0 0 1-.7-.7V6.2a.7.7 0 0 1 .7-.7z",
        "previous": "M18.5 6.1v11.8a.9.9 0 0 1-1.4.75l-8.6-5.9a.9.9 0 0 1 0-1.5l8.6-5.9a.9.9 0 0 1 1.4.75zM6.8 5.5H5.2a.7.7 0 0 0-.7.7v11.6a.7.7 0 0 0 .7.7h1.6a.7.7 0 0 0 .7-.7V6.2a.7.7 0 0 0-.7-.7z",
        "volume": "M11 5 6.5 9H3.5v6h3l4.5 4zM15.5 8.8a4.5 4.5 0 0 1 0 6.4M18.4 6a8.5 8.5 0 0 1 0 12",
        "volume-low": "M11 5 6.5 9H3.5v6h3l4.5 4zM15.5 8.8a4.5 4.5 0 0 1 0 6.4",
        "muted": "M11 5 6.5 9H3.5v6h3l4.5 4zM16 9.5l5 5M21 9.5l-5 5",
        "fullscreen": "M4 9V5a1 1 0 0 1 1-1h4M15 4h4a1 1 0 0 1 1 1v4M20 15v4a1 1 0 0 1-1 1h-4M9 20H5a1 1 0 0 1-1-1v-4",
        "windowed": "M9 4v4a1 1 0 0 1-1 1H4M20 9h-4a1 1 0 0 1-1-1V4M15 20v-4a1 1 0 0 1 1-1h4M4 15h4a1 1 0 0 1 1 1v4",
        "subtitles": "M5 5h14a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2zM7 15h4M14 15h3M7 11.5h2.5M12.5 11.5H17",
        "playlist": "M4 6.5h11M4 11.5h11M4 16.5h7M17 14v6l4.5-3z",
        "folder": "M3.5 7.5a2 2 0 0 1 2-2h3.6l2 2h7.4a2 2 0 0 1 2 2v7.5a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2z",
        "film": "M6 3.5h12a2 2 0 0 1 2 2v13a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-13a2 2 0 0 1 2-2zM8 3.5v17M16 3.5v17M4 8.5h4M4 15.5h4M16 8.5h4M16 15.5h4",
        "back": "M19 12H5M11 18l-6-6 6-6",
        "close": "M6.5 6.5l11 11M17.5 6.5l-11 11",
        "check": "M5 12.5l4.5 4.5L19 7.5",
        "music": "M9 18V5.5l11-2V16M9 18a3 3 0 1 1-6 0 3 3 0 0 1 6 0zM20 16a3 3 0 1 1-6 0 3 3 0 0 1 6 0z",
        "speed": "M12 13.5l3.8-3.8M4.2 18a9 9 0 1 1 15.6 0",
        "help": "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM9.6 9.4a2.5 2.5 0 1 1 3.6 2.3c-.7.3-1.2.9-1.2 1.7v.4M12 17h.01",
        "plus": "M12 5v14M5 12h14",
        "replay": "M3.5 12a8.5 8.5 0 1 0 2.6-6.1L3.5 8.5M3.5 3.5v5h5",
        "forward": "M13 6l6 6-6 6M5 6l6 6-6 6",
        "rewind": "M11 6l-6 6 6 6M19 6l-6 6 6 6",
        "link": "M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1",
        "camera": "M4 8a2 2 0 0 1 2-2h2l1.5-2h5L16 6h2a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2zM12 16.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7z",
        "audio": "M4 10v4M8 7v10M12 4v16M16 8v8M20 11v2",
        "warning": "M12 4 2.8 19.5h18.4zM12 10v4.5M12 17h.01"
    })
    readonly property var solid: ({ "play": true, "pause": true, "next": true, "previous": true })
    readonly property bool isSolid: solid[name] === true

    Shape {
        width: 24
        height: 24
        anchors.centerIn: parent
        scale: Math.min(glyph.width, glyph.height) / 24
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: glyph.isSolid ? glyph.color : "transparent"
            strokeColor: glyph.isSolid ? "transparent" : glyph.color
            strokeWidth: glyph.isSolid ? 0 : glyph.lineWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: glyph.paths[glyph.name] || "" }
        }
    }
}
