import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property var tab: null
    property string connectionLabel
    color: Theme.bg

    Connections {
        target: root.tab
        function onTextChanged() {
            if (root.tab && editor.text !== root.tab.editorText) editor.text = root.tab.editorText
        }
    }

    Item {
        anchors { fill: parent; margins: Theme.margin; topMargin: 8 }

        Row {
            id: toolbar
            width: parent.width
            height: 40
            spacing: 8
            Text {
                width: parent.width - runButtons.width
                text: root.connectionLabel
                color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
            Row {
                id: runButtons
                spacing: 2
                NavButton {
                    text: root.tab && root.tab.busy ? qsTr("Cancel") : qsTr("Run")
                    bold: true
                    color: root.tab && root.tab.busy ? Theme.danger : Theme.accent
                    onClicked: {
                        if (!root.tab) return
                        if (root.tab.busy) { root.tab.cancel(); return }
                        if (root.tab.needsConfirm()) confirmSheet.open()
                        else root.tab.run()
                    }
                }
                NavButton { icon: "share"; active: root.tab && root.tab.grid.rowCount > 0
                            onClicked: platform.shareText("query.csv", root.tab.grid.toCsv(), "text/csv") }
                NavButton { text: qsTr("Clear"); onClicked: if (root.tab) root.tab.clearResult() }
                NavButton { text: qsTr("History"); onClicked: { if (root.tab) root.tab.loadHistory(); historySheet.open() } }
            }
        }

        Rectangle {
            id: editorBox
            anchors { top: toolbar.bottom; left: parent.left; right: parent.right; topMargin: 8 }
            height: Math.max(110, parent.height * 0.32)
            radius: Theme.radius
            color: Theme.card
            TextArea {
                id: editor
                anchors.fill: parent
                anchors.margins: 8
                text: root.tab ? root.tab.editorText : ""
                onTextChanged: if (root.tab) root.tab.editorText = text
                color: Theme.text
                background: null
                font { pixelSize: 14; family: "monospace" }
                wrapMode: TextArea.WrapAnywhere
                placeholderText: qsTr("SELECT * FROM ...")
                placeholderTextColor: Theme.text2
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }
        }

        Text {
            id: message
            anchors { top: editorBox.bottom; left: parent.left; right: parent.right; topMargin: 6 }
            height: visible ? implicitHeight : 0
            visible: text !== ""
            text: root.tab ? root.tab.message : ""
            color: Theme.text2; font.pixelSize: 12; wrapMode: Text.Wrap
        }

        GridTable {
            anchors { top: message.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
            anchors.topMargin: 6
            grid: root.tab ? root.tab.grid : null
        }
    }

    ActionSheet {
        id: confirmSheet
        title: qsTr("Run this statement?")
        message: root.tab ? root.tab.editorText : ""
        actions: [{ text: qsTr("Run"), destructive: true }]
        onChosen: root.tab.run()
    }

    Popup {
        id: historySheet
        parent: Overlay.overlay
        modal: true
        dim: true
        x: (parent.width - width) / 2
        y: parent.height - height - Theme.safeBottom - 8
        width: Math.min(parent.width - 16, 560)
        height: Math.min(parent.height - 80, 420)
        padding: 0
        background: Rectangle { radius: 16; color: Theme.card }

        contentItem: Column {
            width: historySheet.width
            Item {
                width: parent.width; height: 48
                Text { anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                       text: qsTr("Query history"); color: Theme.text; font { pixelSize: 18; weight: Font.DemiBold } }
                Text { anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                       text: qsTr("Close"); color: Theme.accent; font.pixelSize: 16
                       MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: historySheet.close() } }
            }
            ListView {
                width: parent.width
                height: historySheet.height - 48
                clip: true
                model: root.tab ? root.tab.history : []
                delegate: Item {
                    width: parent.width; height: 52
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 0.5; color: Theme.sep }
                    Text {
                        anchors { left: parent.left; leftMargin: 16; right: trash.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: modelData.sql
                        color: Theme.text; font { pixelSize: 13; family: "monospace" }
                        elide: Text.ElideRight
                    }
                    Item { id: trash; width: 36; height: 36
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        Icon { anchors.centerIn: parent; name: "trash"; size: 18; color: Theme.danger }
                        MouseArea { anchors.fill: parent; onClicked: root.tab.deleteHistory(index) }
                    }
                    MouseArea {
                        anchors { left: parent.left; right: trash.left; top: parent.top; bottom: parent.bottom }
                        onClicked: { root.tab.useHistory(index); historySheet.close() }
                    }
                }
            }
        }
    }
}
