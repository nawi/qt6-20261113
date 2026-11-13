import QtQuick

// Row inside Group. Children go to the trailing area (right side).
Item {
    id: cell
    property string title
    property string value
    property string icon
    property color iconColor: Theme.accent
    property bool chevron: false
    property bool destructive: false
    property bool tappable: true
    property bool isFirst: true
    property bool isLast: true
    signal clicked()
    default property alias trailing: trail.data

    width: parent ? parent.width : 0
    implicitHeight: 52
    height: visible ? implicitHeight : 0
    onVisibleChanged: if (parent && parent.relayout) Qt.callLater(parent.relayout)

    Rectangle {
        anchors.fill: parent
        visible: ma.pressed && cell.tappable
        color: Theme.pressed
        topLeftRadius: cell.isFirst ? Theme.radius : 0
        topRightRadius: cell.isFirst ? Theme.radius : 0
        bottomLeftRadius: cell.isLast ? Theme.radius : 0
        bottomRightRadius: cell.isLast ? Theme.radius : 0
    }
    Rectangle {
        visible: !cell.isFirst
        anchors { top: parent.top; right: parent.right }
        width: parent.width - Theme.margin; height: 0.5
        color: Theme.sep
    }
    Row {
        anchors { left: parent.left; leftMargin: Theme.margin; verticalCenter: parent.verticalCenter }
        spacing: 12
        Icon { visible: cell.icon !== ""; name: cell.icon; color: cell.iconColor; size: 22; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: cell.title
            color: cell.destructive ? Theme.danger : Theme.text
            font.pixelSize: 17
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    Row {
        anchors { right: parent.right; rightMargin: Theme.margin; verticalCenter: parent.verticalCenter }
        spacing: 8
        Text {
            visible: cell.value !== ""
            text: cell.value
            color: Theme.text2
            font.pixelSize: 17
            anchors.verticalCenter: parent.verticalCenter
        }
        Item { id: trail; width: childrenRect.width; height: 28; anchors.verticalCenter: parent.verticalCenter }
        Icon { visible: cell.chevron; name: "chevronRight"; color: Theme.text2; size: 16; strokeWidth: 2.5; anchors.verticalCenter: parent.verticalCenter }
    }
    MouseArea { id: ma; anchors.fill: parent; enabled: cell.tappable; onClicked: cell.clicked() }
}
