import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property var tab: null
    property string connectionLabel
    color: Theme.bg
    property int editRow: -1
    property int editCol: -1
    property string status

    Connections {
        target: root.tab
        function onWriteFinished(ok, message) { root.status = message }
        function onChanged() {}
    }

    Column {
        anchors { fill: parent; leftMargin: Theme.margin; rightMargin: Theme.margin; topMargin: 8 }
        spacing: 8

        Row {
            width: parent.width
            spacing: 6
            Text {
                width: parent.width - tools.width
                anchors.verticalCenter: parent.verticalCenter
                text: root.tab ? (root.tab.table + "  ·  " + root.tab.grid.columnCount + " " + qsTr("columns")
                                 + (root.tab.readOnly ? "  ·  " + qsTr("read-only") : "  ·  PK"))
                               : ""
                color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight
            }
            Row {
                id: tools
                spacing: 2
                NavButton { icon: "sliders"; onClicked: filterSheet.open() }
                NavButton { icon: "plus"; active: root.tab && !root.tab.readOnly; onClicked: addSheet.open() }
                NavButton { icon: "table"; onClicked: root.tab.openDesign() }
                NavButton { icon: "check"; onClicked: root.tab.refresh() }
            }
        }

        Text {
            width: parent.width
            visible: text !== ""
            text: root.tab ? root.tab.error : ""
            color: Theme.danger; font.pixelSize: 12; wrapMode: Text.Wrap
        }

        GridTable {
            id: grid
            width: parent.width
            height: parent.height - 36 - 40 - (statusText.visible ? 18 : 0)
            grid: root.tab ? root.tab.grid : null
            interactive: true
            onCellTapped: function(row, col) {
                root.editRow = row; root.editCol = col
                editSheet.columnName = root.tab.grid.headers[col]
                editSheet.value = root.tab.grid.cell(row, col) === null ? "" : String(root.tab.grid.cell(row, col))
                editSheet.isNull = root.tab.grid.isNull(row, col)
                editSheet.readOnly = root.tab.readOnly
                editSheet.open()
            }
        }

        Row {
            width: parent.width; height: 34
            NavButton { text: qsTr("Prev"); active: root.tab && root.tab.offset > 0; onClicked: root.tab.prevPage() }
            Item { width: parent.width - 180; height: 1 }
            Text { anchors.verticalCenter: parent.verticalCenter; color: Theme.text2; font.pixelSize: 12
                   text: root.tab ? qsTr("From row %1").arg(root.tab.offset + 1) : "" }
            Item { width: 8; height: 1 }
            NavButton { text: qsTr("Next"); active: root.tab && root.tab.hasMore; onClicked: root.tab.nextPage() }
        }

        Text {
            id: statusText
            width: parent.width
            visible: root.status !== ""
            text: root.status; color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight
        }
    }

    FilterSheet { id: filterSheet; tab: root.tab }
    AddRowSheet { id: addSheet; tab: root.tab; onSaved: function(values) { root.tab.insertRow(values) } }

    CellEditorSheet {
        id: editSheet
        onSaved: function(value, isNull) {
            if (root.tab && root.editRow >= 0)
                root.tab.updateCell(root.editRow, root.tab.grid.headers[root.editCol], value, isNull)
        }
        onDeleteRequested: {
            deleteConfirm.message = root.tab.deletePreview(root.editRow)
            deleteConfirm.open()
        }
    }

    ActionSheet {
        id: deleteConfirm
        title: qsTr("Delete this row?")
        actions: [{ text: qsTr("Delete"), destructive: true }]
        onChosen: root.tab.deleteRow(root.editRow)
    }
}
