import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property var tab: null
    property string connectionLabel
    property string connectionStatus
    property bool connectionError: false
    color: Theme.bg
    property int editRow: -1
    property int editCol: -1
    property string status

    function exportCsv() {
        if (!root.tab) return
        var ok = platform.shareText(root.tab.table + ".csv", root.tab.grid.toCsv(), "text/csv")
        root.status = ok ? qsTr("Exported %1 rows (current page).").arg(root.tab.grid.rowCount)
                         : qsTr("Export is only available on Android.")
    }

    Connections {
        target: root.tab
        function onWriteFinished(ok, message) { root.status = message }
        function onChanged() {}
    }

    Column {
        anchors { fill: parent; leftMargin: Theme.margin; rightMargin: Theme.margin; topMargin: 8 }
        spacing: 8

        // header: left = table info (2 rows), right = connection info (2 rows, mirrors top strip)
        Column {
            id: header
            width: parent.width
            Row {
                width: parent.width
                Text {
                    width: parent.width / 2
                    text: root.tab ? root.tab.table : ""
                    color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight
                }
                Text {
                    width: parent.width / 2
                    horizontalAlignment: Text.AlignRight
                    text: root.connectionLabel
                    color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight
                }
            }
            Row {
                width: parent.width
                Text {
                    width: parent.width / 2
                    text: root.tab ? (root.tab.grid.columnCount + " " + qsTr("columns")
                                     + (root.tab.readOnly ? "  ·  " + qsTr("read-only") : "  ·  PK"))
                                   : ""
                    color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight
                }
                Text {
                    width: parent.width / 2
                    horizontalAlignment: Text.AlignRight
                    text: root.connectionStatus
                    color: root.connectionError ? Theme.danger : Theme.text2
                    font.pixelSize: 12; elide: Text.ElideRight
                }
            }
        }

        Row {
            id: tools
            width: parent.width
            spacing: 2
            NavButton { icon: "search"; color: root.tab && root.tab.search !== "" ? Theme.accent : Theme.text2
                        onClicked: { searchBar.shown = !searchBar.shown; if (!searchBar.shown) root.tab.setSearch("") } }
            NavButton { icon: "share"; onClicked: root.exportCsv() }
            NavButton { icon: "sliders"; onClicked: filterSheet.open() }
            NavButton { icon: "plus"; active: root.tab && !root.tab.readOnly; onClicked: addSheet.open() }
            NavButton { icon: "table"; onClicked: root.tab.openDesign() }
            NavButton { icon: "check"; onClicked: root.tab.refresh() }
        }

        Rectangle {
            id: searchBar
            property bool shown: false
            width: parent.width; height: shown ? 40 : 0; visible: shown
            radius: 10; color: Theme.fill
            TextField {
                id: searchField
                anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                placeholderText: qsTr("Search all columns (current filter)")
                placeholderTextColor: Theme.text2
                color: Theme.text; font.pixelSize: 15
                background: null
                inputMethodHints: Qt.ImhNoPredictiveText
                onAccepted: { root.tab.setSearch(text); Qt.inputMethod.hide() }
            }
            onShownChanged: if (shown) searchField.forceActiveFocus()
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
            height: parent.height - header.height - tools.height - 40
                     - (statusText.visible ? 18 : 0) - (searchBar.shown ? 48 : 0)
            grid: root.tab ? root.tab.grid : null
            interactive: true
            sortColumn: root.tab ? root.tab.sortColumn : -1
            sortDesc: root.tab ? root.tab.sortDesc : false
            onHeaderTapped: function(col) { root.tab.toggleSort(col) }
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
