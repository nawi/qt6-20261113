import QtQuick

Item {
    id: sw
    property bool checked: false
    signal toggled()
    width: 51; height: 31
    Rectangle {
        anchors.fill: parent; radius: height / 2
        color: sw.checked ? Theme.success : Theme.fill
        Behavior on color { ColorAnimation { duration: 150 } }
    }
    Rectangle {
        y: 2; width: 27; height: 27; radius: 13.5; color: "white"
        x: sw.checked ? parent.width - width - 2 : 2
        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }
    MouseArea { anchors.fill: parent; onClicked: { sw.checked = !sw.checked; sw.toggled() } }
}
