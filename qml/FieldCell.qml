import QtQuick
import QtQuick.Controls

// Label on the left, editable value on the right.
Item {
    id: f
    property string title
    property string placeholder
    property alias text: input.text
    property bool secret: false
    property bool numeric: false
    property bool isFirst: true
    property bool isLast: true
    property int labelWidth: 112
    signal edited()
    signal committed()

    width: parent ? parent.width : 0
    implicitHeight: 52
    height: visible ? implicitHeight : 0
    onVisibleChanged: if (parent && parent.relayout) Qt.callLater(parent.relayout)

    Rectangle {
        visible: !f.isFirst
        anchors { top: parent.top; right: parent.right }
        width: parent.width - Theme.margin; height: 0.5
        color: Theme.sep
    }
    Text {
        id: lbl
        x: Theme.margin
        width: f.labelWidth
        anchors.verticalCenter: parent.verticalCenter
        text: f.title
        color: Theme.text
        font.pixelSize: 17
        elide: Text.ElideRight
    }
    TextField {
        id: input
        anchors { left: lbl.right; right: parent.right; rightMargin: Theme.margin - 8; verticalCenter: parent.verticalCenter }
        leftPadding: 8; rightPadding: 8
        background: null
        color: Theme.text
        placeholderText: f.placeholder
        placeholderTextColor: Theme.text2
        font.pixelSize: 17
        horizontalAlignment: Text.AlignRight
        echoMode: f.secret ? TextInput.Password : TextInput.Normal
        inputMethodHints: f.numeric ? Qt.ImhDigitsOnly
                                    : (f.secret ? Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                                                : Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText)
        selectByMouse: true
        onDisplayTextChanged: f.edited()
        onEditingFinished: f.committed()
    }
    function focusInput() { input.forceActiveFocus() }
}
