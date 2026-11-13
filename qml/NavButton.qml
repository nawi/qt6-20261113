import QtQuick

// Text and/or icon button for NavBar. Hit area >= 44dp.
Item {
    id: b
    property string text
    property string icon
    property bool bold: false
    property bool active: true
    property color color: Theme.accent
    signal clicked()

    width: Math.max(44, row.width + 16)
    height: 44
    opacity: !active ? 0.4 : (ma.pressed ? 0.5 : 1)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2
        Icon { visible: b.icon !== ""; name: b.icon; color: b.color; size: 22; anchors.verticalCenter: parent.verticalCenter }
        Text {
            visible: b.text !== ""
            text: b.text
            color: b.color
            font { pixelSize: 17; weight: b.bold ? Font.DemiBold : Font.Normal }
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    MouseArea { id: ma; anchors.fill: parent; enabled: b.active; onClicked: b.clicked() }
}
