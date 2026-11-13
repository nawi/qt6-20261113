import QtQuick
import QtQuick.Controls

// Bottom action sheet. actions: [{text, destructive}], signal chosen(index).
Popup {
    id: sheet
    property string title
    property string message
    property var actions: []
    signal chosen(int index)

    parent: Overlay.overlay
    modal: true
    dim: true
    x: (parent.width - width) / 2
    y: parent.height - height - Theme.safeBottom - 8
    width: Math.min(parent.width - 16, 480)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 100 } }

    background: Item {}
    contentItem: Column {
        spacing: 8
        Rectangle {
            width: parent.width
            height: group.height
            radius: 14
            color: Theme.card
            Column {
                id: group
                width: parent.width
                Item {
                    visible: sheet.title !== "" || sheet.message !== ""
                    width: parent.width
                    height: visible ? head.height + 24 : 0
                    Column {
                        id: head
                        anchors { centerIn: parent }
                        width: parent.width - 32
                        spacing: 2
                        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; visible: sheet.title !== ""
                               text: sheet.title; color: Theme.text2; font { pixelSize: 13; weight: Font.DemiBold } }
                        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; visible: sheet.message !== ""
                               text: sheet.message; color: Theme.text2; font.pixelSize: 13; wrapMode: Text.Wrap }
                    }
                }
                Repeater {
                    model: sheet.actions
                    delegate: Item {
                        width: group.width; height: 56
                        Rectangle { width: parent.width; height: 0.5; color: Theme.sep }
                        Rectangle { anchors.fill: parent; color: Theme.pressed; visible: am.pressed; radius: 14 }
                        Text {
                            anchors.centerIn: parent
                            text: modelData.text
                            color: modelData.destructive ? Theme.danger : Theme.accent
                            font.pixelSize: 20
                        }
                        MouseArea { id: am; anchors.fill: parent; onClicked: { sheet.close(); sheet.chosen(index) } }
                    }
                }
            }
        }
        Rectangle {
            width: parent.width; height: 56; radius: 14; color: Theme.card
            opacity: cm.pressed ? 0.7 : 1
            Text { anchors.centerIn: parent; text: qsTr("Cancel"); color: Theme.accent; font { pixelSize: 20; weight: Font.DemiBold } }
            MouseArea { id: cm; anchors.fill: parent; onClicked: sheet.close() }
        }
    }
    Overlay.modal: Rectangle { color: Theme.dark ? "#99000000" : "#55000000" }
}
