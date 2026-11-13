import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    title: "NawiDBMan"

    Label {
        anchors.centerIn: parent
        text: "NawiDBMan\nQt " + Qt.platform.os
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 28
    }
}
