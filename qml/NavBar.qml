import QtQuick

// iOS navigation bar. progress 0..1: how far the large title has scrolled away.
Item {
    id: bar
    property string title
    property real progress: 1
    property alias leading: leftRow.data
    property alias trailing: rightRow.data

    width: parent ? parent.width : 0
    height: Theme.safeTop + 44

    Rectangle { anchors.fill: parent; color: Theme.bg }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width; height: 0.5
        color: Theme.sep
        opacity: bar.progress
    }
    Row {
        id: leftRow
        anchors { left: parent.left; leftMargin: Theme.safeLeft + 8; bottom: parent.bottom }
        height: 44
    }
    Text {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
        height: 44
        verticalAlignment: Text.AlignVCenter
        width: parent.width - 2 * 110
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: bar.title
        font { pixelSize: 17; weight: Font.DemiBold }
        color: Theme.text
        opacity: bar.progress
    }
    Row {
        id: rightRow
        anchors { right: parent.right; rightMargin: Theme.safeRight + 8; bottom: parent.bottom }
        height: 44
        spacing: 4
    }
}
