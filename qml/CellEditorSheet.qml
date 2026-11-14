import QtQuick
import QtQuick.Controls

// Cell editor: text value + explicit NULL toggle (NULL is not the same as '').
Popup {
    id: sheet
    property string columnName
    property string value
    property bool isNull: false
    property bool readOnly: false
    signal saved(string value, bool isNull)
    signal deleteRequested()

    parent: Overlay.overlay
    modal: true
    dim: true
    x: (parent.width - width) / 2
    y: Math.max(Theme.safeTop + 8, (parent.height - Theme.safeIme - height) / 2)
    width: Math.min(parent.width - 32, 480)
    padding: 16
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { radius: 16; color: Theme.card }

    onAboutToShow: input.text = sheet.isNull ? "" : sheet.value
    onValueChanged: if (!sheet.isNull) input.text = sheet.value
    onIsNullChanged: if (sheet.isNull) input.text = ""

    contentItem: Column {
        spacing: 14
        Text { width: parent.width; text: sheet.columnName; color: Theme.text2
               font { pixelSize: 13; weight: Font.DemiBold; capitalization: Font.AllUppercase } }
        Rectangle {
            width: parent.width
            height: 140
            radius: 10
            color: Theme.fill
            clip: true

            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                flickableDirection: Flickable.VerticalFlick
                ScrollBar.vertical: ScrollBar {}

                TextArea.flickable: TextArea {
                    id: input
                    width: flick.width
                    enabled: !sheet.isNull && !sheet.readOnly
                    color: Theme.text
                    background: null
                    font { pixelSize: 15; family: "monospace" }
                    wrapMode: TextArea.WrapAnywhere
                    selectByMouse: true
                }
            }
        }
        Row {
            width: parent.width
            Text { text: qsTr("Set NULL"); color: Theme.text; font.pixelSize: 16
                   anchors.verticalCenter: parent.verticalCenter }
            Item { width: parent.width - 140; height: 1 }
            IOSSwitch { checked: sheet.isNull; enabled: !sheet.readOnly
                        onToggled: sheet.isNull = checked }
        }
        Row {
            width: parent.width; spacing: 12
            PrimaryButton {
                width: (parent.width - 12) / 2; prominent: false
                text: qsTr("Cancel"); onClicked: sheet.close()
            }
            PrimaryButton {
                width: (parent.width - 12) / 2; text: qsTr("Save"); active: !sheet.readOnly
                onClicked: { sheet.saved(input.text, sheet.isNull); sheet.close() }
            }
        }
        PrimaryButton {
            width: parent.width; prominent: false; tint: Theme.danger
            text: qsTr("Delete Row"); visible: !sheet.readOnly
            onClicked: { sheet.close(); sheet.deleteRequested() }
        }
    }
}
