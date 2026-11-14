import QtQuick
import QtQuick.Shapes

// Stroke icons (24x24 grid, Lucide-style paths).
Item {
    id: ic
    property string name
    property color color: Theme.accent
    property real size: 22
    property real strokeWidth: 2
    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "chevronLeft": "M15 18 L9 12 L15 6",
        "chevronRight": "M9 18 L15 12 L9 6",
        "plus": "M12 5 L12 19 M5 12 L19 12",
        "check": "M20 6 L9 17 L4 12",
        "pencil": "M12 20h9 M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4Z",
        "sliders": "M4 21v-7 M4 10V3 M12 21v-9 M12 8V3 M20 21v-5 M20 12V3 M1 14h6 M9 8h6 M17 16h6",
        "trash": "M3 6h18 M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6 M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2",
        "database": "M12 3C7 3 3 4.3 3 6s4 3 9 3 9-1.3 9-3-4-3-9-3Z M3 6v12c0 1.7 4 3 9 3s9-1.3 9-3V6 M3 12c0 1.7 4 3 9 3s9-1.3 9-3",
        "shield": "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z",
        "globe": "M12 2a10 10 0 1 0 0.01 0 M2 12h20 M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z",
        "link": "M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71 M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71",
        "table": "M3 5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2Z M3 10h18 M3 15h18 M9 10v11",
        "share": "M4 12v8a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-8 M16 6l-4-4-4 4 M12 2v13",
        "search": "M11 19a8 8 0 1 0 0-16 8 8 0 0 0 0 16z M21 21l-4.35-4.35",
        "lock": "M5 11h14v10H5Z M8 11V7a4 4 0 0 1 8 0v4",
        "refresh": "M21 2v6h-6 M3 22v-6h6 M3 11a9 9 0 0 1 15-6.7L21 8 M21 13a9 9 0 0 1-15 6.7L3 16"
    })

    Shape {
        width: 24; height: 24
        scale: ic.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: ic.color
            strokeWidth: ic.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: ic.paths[ic.name] || "" }
        }
    }
}
