import QtQuick
import QtQuick.Controls

// Filter builder (F7): column + operator + value, AND/OR join.
Popup {
    id: sheet
    property var tab: null
    property var ops: ["=", "!=", "<", ">", "<=", ">=", "LIKE", "IS NULL", "IS NOT NULL", "IN"]
    signal applied()

    parent: Overlay.overlay
    modal: true
    dim: true
    x: (parent.width - width) / 2
    y: parent.height - height - Theme.safeBottom - 8
    width: Math.min(parent.width - 16, 560)
    height: Math.min(parent.height - 80, 190 + Math.min(condModel.count * 52, 260))
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        condModel.clear()
        var f = tab ? tab.filters : []
        for (var i = 0; i < f.length; ++i) {
            condModel.append({ col: f[i].column, op: f[i].op, val: f[i].value, join: f[i].join || "AND" })
        }
    }

    background: Rectangle { radius: 16; color: Theme.card }

    contentItem: Column {
        id: content
        width: sheet.width
        spacing: 12

        Item {
            width: parent.width; height: 48
            Text { anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                   text: qsTr("Filter rows"); color: Theme.text; font { pixelSize: 18; weight: Font.DemiBold } }
            Text { anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                   text: qsTr("Done"); color: Theme.accent; font.pixelSize: 16
                   MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: {
                       var out = []
                       for (var i = 0; i < condModel.count; ++i) {
                           var c = condModel.get(i)
                           if (c.col === "") continue
                           out.push({ column: c.col, op: c.op, value: c.val, join: i === 0 ? "AND" : c.join })
                       }
                       if (tab) tab.setFilters(out)
                       sheet.applied(); sheet.close()
                   } } }
        }

        ListView {
            id: list
            width: parent.width
            height: Math.min(condModel.count * 52, 4 * 52)   // fixed cap: must not depend on sheet.height (binding loop)
            model: condModel
            clip: true

            delegate: Item {
                id: rowDelegate
                property int rowIndex: index
                width: list.width; height: 52
                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            height: 0.5; color: Theme.sep }
                Row {
                    anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                    spacing: 6
                    ComboBox {
                        id: joinBox
                        width: 54
                        model: ["AND", "OR"]
                        currentIndex: model.join === "OR" ? 1 : 0
                        onActivated: condModel.setProperty(rowDelegate.rowIndex, "join", currentText)
                        visible: rowDelegate.rowIndex > 0
                    }
                    ComboBox {
                        id: colBox
                        width: 96
                        model: sheet.tab ? sheet.tab.grid.headers : []
                        currentIndex: Math.max(0, (sheet.tab ? sheet.tab.grid.headers : []).indexOf(condModel.get(rowDelegate.rowIndex).col))
                        onActivated: condModel.setProperty(rowDelegate.rowIndex, "col", currentText)
                    }
                    ComboBox {
                        width: 78
                        model: sheet.ops
                        currentIndex: Math.max(0, sheet.ops.indexOf(condModel.get(rowDelegate.rowIndex).op))
                        onActivated: condModel.setProperty(rowDelegate.rowIndex, "op", currentText)
                    }
                    TextField {
                        width: Math.max(64, parent.width - (joinBox.visible ? 60 : 0) - 96 - 78 - 36 - 24)
                        text: condModel.get(rowDelegate.rowIndex).val
                        visible: condModel.get(rowDelegate.rowIndex).op !== "IS NULL" && condModel.get(rowDelegate.rowIndex).op !== "IS NOT NULL"
                        placeholderText: qsTr("Value")
                        onTextEdited: condModel.setProperty(rowDelegate.rowIndex, "val", text)
                    }
                    Item { width: 32; height: 36
                        Icon { anchors.centerIn: parent; name: "trash"; color: Theme.danger; size: 18 }
                        MouseArea { anchors.fill: parent; onClicked: condModel.remove(rowDelegate.rowIndex) }
                    }
                }
            }
        }

        Item {
            width: parent.width; height: 48
            Text { anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                   text: qsTr("+ Add condition"); color: Theme.accent; font.pixelSize: 15
                   MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked:
                       condModel.append({ col: sheet.tab && sheet.tab.grid.columnCount > 0 ? sheet.tab.grid.headers[0] : "",
                                          op: "=", val: "", join: "AND" }) } }
        }

        Row {
            width: parent.width - 32
            x: 16
            spacing: 12
            PrimaryButton { width: (parent.width - 12) / 2; prominent: false; text: qsTr("Clear")
                            onClicked: { if (sheet.tab) sheet.tab.clearFilters(); sheet.close() } }
            PrimaryButton { width: (parent.width - 12) / 2; text: qsTr("Apply"); onClicked: {
                var out = []
                for (var i = 0; i < condModel.count; ++i) {
                    var c = condModel.get(i)
                    if (c.col === "") continue
                    out.push({ column: c.col, op: c.op, value: c.val, join: i === 0 ? "AND" : c.join })
                }
                if (sheet.tab) sheet.tab.setFilters(out)
                sheet.applied(); sheet.close()
            } }
        }

        Item { width: 1; height: 4 }
    }

    ListModel { id: condModel }
}
