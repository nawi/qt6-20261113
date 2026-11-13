import QtQuick

// iOS segmented control.
Item {
    id: s
    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    implicitHeight: 34
    height: implicitHeight

    Rectangle { anchors.fill: parent; radius: 9; color: Theme.fill }
    Rectangle {
        id: thumb
        y: 2; height: parent.height - 4
        width: (parent.width - 4) / Math.max(1, s.model.length)
        x: 2 + s.currentIndex * width
        radius: 7
        color: Theme.dark ? "#636366" : "#FFFFFF"
        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }
    Row {
        anchors { fill: parent; margins: 2 }
        Repeater {
            model: s.model
            delegate: Item {
                width: (s.width - 4) / s.model.length; height: s.height - 4
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: Theme.text
                    font { pixelSize: 14; weight: index === s.currentIndex ? Font.DemiBold : Font.Normal }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s.currentIndex = index; s.activated(index) }
                }
            }
        }
    }
}
