import QtQuick
import QtQuick.Controls

Item {
    id: b
    property string text
    property bool busy: false
    property bool active: true
    property bool prominent: true     // filled accent, else tinted
    property color tint: Theme.accent
    signal clicked()

    implicitHeight: 50
    height: implicitHeight
    opacity: !active ? 0.45 : (ma.pressed ? 0.7 : 1)

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: b.prominent ? b.tint : Qt.rgba(b.tint.r, b.tint.g, b.tint.b, 0.15)
    }
    Text {
        anchors.centerIn: parent
        visible: !b.busy
        text: b.text
        color: b.prominent ? Theme.textOnAccent : b.tint
        font { pixelSize: 17; weight: Font.DemiBold }
    }
    BusyIndicator {
        anchors.centerIn: parent
        running: b.busy; visible: b.busy
        width: 28; height: 28
    }
    MouseArea { id: ma; anchors.fill: parent; enabled: b.active && !b.busy; onClicked: b.clicked() }
}
