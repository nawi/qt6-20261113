import QtQuick

// Inset grouped section (iOS Settings style) with optional header/footer text.
Item {
    id: g
    property string header
    property string footer
    default property alias content: col.data
    width: parent ? parent.width : 0
    implicitHeight: (hdr.visible ? hdr.height + 6 : 0) + col.height + (ftr.visible ? ftr.height + 6 : 0)
    height: implicitHeight

    Text {
        id: hdr
        visible: g.header !== ""
        text: g.header.toUpperCase()
        width: parent.width - 2 * Theme.margin
        x: Theme.margin
        height: visible ? contentHeight + 6 : 0
        color: Theme.text2
        font.pixelSize: 13
        verticalAlignment: Text.AlignBottom
        wrapMode: Text.Wrap
    }
    Rectangle {
        anchors { top: hdr.visible ? hdr.bottom : parent.top; topMargin: hdr.visible ? 6 : 0 }
        width: parent.width; height: col.height
        color: Theme.card
        radius: Theme.radius
    }
    Column {
        id: col
        anchors { top: hdr.visible ? hdr.bottom : parent.top; topMargin: hdr.visible ? 6 : 0 }
        width: parent.width

        function relayout() {
            var first = null, last = null
            for (var i = 0; i < children.length; ++i) {
                var c = children[i]
                if (c.visible && c.hasOwnProperty("isFirst")) { if (!first) first = c; last = c }
            }
            for (i = 0; i < children.length; ++i) {
                c = children[i]
                if (c.hasOwnProperty("isFirst")) { c.isFirst = (c === first); c.isLast = (c === last) }
            }
        }
        onChildrenChanged: Qt.callLater(relayout)
        Component.onCompleted: relayout()
    }
    Text {
        id: ftr
        visible: g.footer !== ""
        text: g.footer
        anchors { top: col.bottom; topMargin: 6 }
        x: Theme.margin
        width: parent.width - 2 * Theme.margin
        color: Theme.text2
        font.pixelSize: 13
        wrapMode: Text.Wrap
    }
}
