import QtQuick

// Horizontally + vertically scrollable read-only grid. `grid` is a C++ GridModel.
Item {
    id: root
    property var grid: null
    property bool interactive: false
    signal cellTapped(int row, int column)

    readonly property var widths: grid ? grid.widths : []
    readonly property real totalWidth: {
        var w = 0
        if (widths) for (var i = 0; i < widths.length; ++i) w += widths[i]
        return w
    }

    Rectangle { anchors.fill: parent; color: Theme.card; radius: Theme.radius; clip: true }

    Flickable {
        id: hflick
        anchors { fill: parent; margins: 1 }
        contentWidth: Math.max(width, root.totalWidth)
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick

        Column {
            width: root.totalWidth
            height: hflick.height

            Rectangle {
                width: parent.width; height: 36
                color: Theme.fill
                Row {
                    Repeater {
                        model: root.grid ? root.grid.columnCount : 0
                        Rectangle {
                            width: root.widths[index] || 80; height: 36
                            color: "transparent"
                            Text {
                                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                width: parent.width - 16
                                text: root.grid.headers[index]
                                color: Theme.text2
                                font { pixelSize: 12; weight: Font.DemiBold; capitalization: Font.AllUppercase }
                                elide: Text.ElideRight
                            }
                            Rectangle { anchors.right: parent.right; width: 0.5; height: parent.height; color: Theme.sep }
                        }
                    }
                }
            }

            ListView {
                id: rows
                width: parent.width
                height: parent.height - 36
                model: root.grid
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    id: rowItem
                    property int rowIndex: index
                    property var cells: model.row
                    width: rows.width; height: 44

                    Row {
                        Repeater {
                            model: root.grid ? root.grid.columnCount : 0
                            Rectangle {
                                width: root.widths[index] || 80; height: 44
                                color: root.interactive && ma.pressed ? Theme.pressed : "transparent"
                                Text {
                                    anchors { left: parent.left; leftMargin: 8; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                                    text: {
                                        var v = rowItem.cells ? rowItem.cells[index] : null
                                        return (v === null || v === undefined) ? "NULL" : String(v)
                                    }
                                    color: (rowItem.cells && (rowItem.cells[index] === null || rowItem.cells[index] === undefined))
                                           ? Theme.text2 : Theme.text
                                    font { pixelSize: 14; italic: rowItem.cells && (rowItem.cells[index] === null || rowItem.cells[index] === undefined) }
                                    elide: Text.ElideRight
                                }
                                Rectangle { anchors.right: parent.right; width: 0.5; height: parent.height; color: Theme.sep }
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 0.5; color: Theme.sep }
                                MouseArea {
                                    id: ma
                                    anchors.fill: parent
                                    enabled: root.interactive
                                    onClicked: root.cellTapped(rowItem.rowIndex, index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        visible: !root.grid || root.grid.rowCount === 0
        spacing: 8
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("No rows")
               color: Theme.text2; font.pixelSize: 16 }
    }
}
