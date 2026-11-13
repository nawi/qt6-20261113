import QtQuick

// Scrolling form container: shrinks above the on-screen keyboard and keeps the focused field visible.
Flickable {
    id: f
    default property alias content: holder.data
    property real contentPadding: 48

    anchors.bottomMargin: Theme.safeIme
    contentWidth: width
    contentHeight: holder.childrenRect.height + (Theme.safeIme > 0 ? 24 : Theme.safeBottom + contentPadding)
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    interactive: contentHeight > height
    onMovementStarted: Qt.inputMethod.hide()

    Item { id: holder; width: f.width }

    function ensureVisible() {
        const it = Window.window ? Window.window.activeFocusItem : null
        if (!it || !it.visible) return
        // walk up to check it lives inside this flickable
        for (var p = it; p; p = p.parent) if (p === holder) break
        if (!p) return
        const y = it.mapToItem(holder, 0, 0).y
        if (y < contentY) contentY = Math.max(0, y - 16)
        else if (y + it.height > contentY + height) contentY = Math.min(contentHeight - height, y + it.height - height + 16)
    }
    Connections { target: Window.window; function onActiveFocusItemChanged() { Qt.callLater(f.ensureVisible) } }
    Connections { target: Theme; function onSafeImeChanged() { Qt.callLater(f.ensureVisible) } }
}
