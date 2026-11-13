pragma Singleton
import QtQuick

// iOS-style palette. Accent follows Android dynamic color (fallback: iOS blue).
QtObject {
    property var settings: null   // injected by Main.qml (singletons cannot see context properties)
    property var platform: null

    readonly property string mode: settings ? settings.theme : "system"
    readonly property bool dark: mode === "dark"
                                 || (mode === "system" && Application.styleHints.colorScheme === Qt.Dark)

    readonly property color accent: {
        var h = platform ? platform.accentHex(dark) : ""
        return h !== "" ? h : (dark ? "#0A84FF" : "#007AFF")
    }
    readonly property color bg: dark ? "#000000" : "#F2F2F7"
    readonly property color card: dark ? "#1C1C1E" : "#FFFFFF"
    readonly property color fill: dark ? "#2C2C2E" : "#E9E9EB"
    readonly property color pressed: dark ? "#2C2C2E" : "#E5E5EA"
    readonly property color text: dark ? "#FFFFFF" : "#000000"
    readonly property color text2: dark ? "#98989F" : "#6C6C70"
    readonly property color sep: dark ? "#38383A" : "#D1D1D6"
    readonly property color danger: dark ? "#FF453A" : "#FF3B30"
    readonly property color success: dark ? "#32D74B" : "#34C759"
    // pick readable text color for filled accent buttons (dynamic color can be very light)
    readonly property color textOnAccent: (0.299 * accent.r + 0.587 * accent.g + 0.114 * accent.b) > 0.6 ? "#101114" : "#FFFFFF"

    readonly property real radius: 12
    readonly property real margin: 16
    readonly property real maxWidth: 640   // content column cap for tablets/landscape

    readonly property real safeTop: platform ? platform.insets.top : 0
    readonly property real safeBottom: platform ? platform.insets.bottom : 0
    readonly property real safeLeft: platform ? platform.insets.left : 0
    readonly property real safeIme: platform ? platform.insets.ime : 0   // on-screen keyboard height
    readonly property real safeRight: platform ? platform.insets.right : 0

    readonly property var labelColors: ["#007AFF", "#34C759", "#FF9500", "#FF3B30",
                                        "#AF52DE", "#30B0C7", "#FF2D55", "#8E8E93"]
}
