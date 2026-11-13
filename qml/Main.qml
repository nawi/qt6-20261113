import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    title: "NawiDBMan"

    Label {
        anchors.centerIn: parent
        width: parent.width - 48
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: 18
        text: "NawiDBMan\n\nMariaDB Connector/C: " + buildInfo.mariadb
              + "\n" + buildInfo.openssl + "\n" + buildInfo.ssh
    }
}
