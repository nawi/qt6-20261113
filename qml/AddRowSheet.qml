import QtQuick
import QtQuick.Controls

// Insert row form. Columns are included only when their "Include" switch is on,
// so omitted columns keep server defaults (and auto-increment keys work).
Popup {
    id: sheet
    property var tab: null
    signal saved(var values)

    parent: Overlay.overlay
    modal: true
    dim: true
    x: (parent.width - width) / 2
    y: parent.height - height - Theme.safeBottom - 8
    width: Math.min(parent.width - 16, 560)
    height: Math.min(parent.height - 80, 120 + list.contentHeight + 92)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        form.clear()
        if (!tab) return
        for (var i = 0; i < tab.grid.columnCount; ++i)
            form.append({ col: tab.grid.headers[i], typ: tab.grid.types[i] || "", include: false,
                          isNull: false, val: "" })
    }

    background: Rectangle { radius: 16; color: Theme.card }

    contentItem: Column {
        width: sheet.width
        spacing: 8

        Item {
            width: parent.width; height: 52
            Text { anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                   text: qsTr("Insert row"); color: Theme.text; font { pixelSize: 18; weight: Font.DemiBold } }
            Text { anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                   text: qsTr("Cancel"); color: Theme.text2; font.pixelSize: 16
                   MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: sheet.close() } }
        }

        ListView {
            id: list
            width: parent.width
            height: Math.min(form.count * 60, sheet.height - 190)
            model: form
            clip: true
            delegate: Item {
                id: row
                property int rowIndex: index
                width: list.width; height: 60
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 0.5; color: Theme.sep }
                Row {
                    anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                    spacing: 8
                    Column {
                        width: 90; anchors.verticalCenter: parent.verticalCenter
                        Text { text: col; color: Theme.text; font.pixelSize: 14; elide: Text.ElideRight; width: parent.width }
                        Text { text: typ; color: Theme.text2; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                    }
                    IOSSwitch { checked: include; anchors.verticalCenter: parent.verticalCenter
                                onToggled: form.setProperty(row.rowIndex, "include", checked) }
                    TextField {
                        width: Math.max(60, parent.width - 90 - 44 - 44 - 34 - 4 * 8)
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: include && !isNull
                        text: val
                        placeholderText: isNull ? "NULL" : qsTr("Value")
                        onTextEdited: form.setProperty(row.rowIndex, "val", text)
                    }
                    Text { text: qsTr("NULL"); color: Theme.text2; font.pixelSize: 12
                           anchors.verticalCenter: parent.verticalCenter }
                    IOSSwitch { checked: isNull; enabled: include; anchors.verticalCenter: parent.verticalCenter
                                onToggled: form.setProperty(row.rowIndex, "isNull", checked) }
                }
            }
        }

        PrimaryButton {
            x: 16; width: parent.width - 32; text: qsTr("Insert")
            onClicked: {
                var out = {}
                for (var i = 0; i < form.count; ++i) {
                    var r = form.get(i)
                    if (!r.include) continue
                    out[r.col] = r.isNull ? null : r.val
                }
                sheet.saved(out)
                sheet.close()
            }
        }
        Item { width: 1; height: 8 }
    }

    ListModel { id: form }

}
