import QtQuick

Rectangle {
    id: t
    function show(msg) { label.text = msg; opacity = 1; hide.restart() }
    opacity: 0
    visible: opacity > 0
    radius: 20
    color: Theme.dark ? "#3A3A3C" : "#CC1C1C1E"
    width: Math.min(label.implicitWidth + 40, parent.width - 32)
    height: label.implicitHeight + 24
    anchors.horizontalCenter: parent.horizontalCenter
    Behavior on opacity { NumberAnimation { duration: 180 } }
    Timer { id: hide; interval: 2600; onTriggered: t.opacity = 0 }
    Text {
        id: label
        anchors.centerIn: parent
        width: parent.width - 40
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: "white"
        font.pixelSize: 15
    }
}
