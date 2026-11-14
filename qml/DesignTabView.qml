import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property var tab: null
    property string connectionLabel
    color: Theme.bg

    property int section: 0

    Column {
        anchors { fill: parent; margins: Theme.margin; topMargin: 8 }
        spacing: 8

        Segmented {
            width: parent.width
            model: [qsTr("Fields"), qsTr("Indexes"), qsTr("DDL")]
            currentIndex: root.section
            onActivated: root.section = index
        }

        Text {
            width: parent.width; visible: text !== ""
            text: root.tab ? (root.tab.objectKind + " · " + root.tab.refName) : ""
            color: Theme.text2; font.pixelSize: 12
        }

        GridTable {
            width: parent.width
            height: parent.height - 34 - 24
            visible: root.section !== 2
            grid: root.tab ? (root.section === 0 ? root.tab.fields : root.tab.indexes) : null
        }

        Rectangle {
            width: parent.width
            height: parent.height - 34 - 24
            visible: root.section === 2
            radius: Theme.radius
            color: Theme.card
            clip: true
            ScrollView {
                id: ddlScroll
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                TextArea {
                    id: ddlArea
                    width: ddlScroll.availableWidth
                    readOnly: true
                    selectByMouse: true
                    text: root.tab ? root.tab.ddlText : ""
                    color: Theme.text
                    background: null
                    font { pixelSize: 12; family: "monospace" }
                    wrapMode: TextArea.WrapAnywhere
                }
            }
            NavButton {
                anchors { right: parent.right; top: parent.top; margins: 4 }
                text: qsTr("Copy")
                onClicked: { ddlArea.selectAll(); ddlArea.copy(); ddlArea.deselect() }
            }
        }
    }
}
