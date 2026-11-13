import QtQuick
import QtQuick.Controls

Rectangle {
    id: page
    color: Theme.bg
    signal finished()

    readonly property var slides: [
        { icon: "database", title: qsTr("Your databases, in your pocket"),
          text: qsTr("Browse and edit MariaDB and MySQL servers from your phone or tablet.") },
        { icon: "shield", title: qsTr("Connect the safe way"),
          text: qsTr("Reach servers directly, through an SSH tunnel, or via a PHP tunnel agent. Credentials are kept encrypted on this device.") },
        { icon: "table", title: qsTr("Work with your data"),
          text: qsTr("Inspect table structure, edit rows, filter results and run SQL in multiple tabs.") }
    ]

    SwipeView {
        id: view
        anchors { fill: parent; topMargin: Theme.safeTop + 44; bottomMargin: Theme.safeBottom + 150 }
        Repeater {
            model: page.slides
            delegate: Item {
                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 2 * 32, 480)
                    spacing: 20
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 112; height: 112; radius: 28
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        Icon { anchors.centerIn: parent; name: modelData.icon; size: 56; strokeWidth: 1.6 }
                    }
                    Text {
                        width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                        text: modelData.title; color: Theme.text; font { pixelSize: 28; weight: Font.Bold }
                    }
                    Text {
                        width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                        text: modelData.text; color: Theme.text2; font.pixelSize: 17; lineHeight: 1.15
                    }
                }
            }
        }
    }

    NavBar {
        title: ""
        trailing: NavButton { text: qsTr("Skip"); visible: view.currentIndex < page.slides.length - 1; onClicked: page.finished() }
    }

    Row {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: Theme.safeBottom + 100 }
        spacing: 8
        Repeater {
            model: page.slides.length
            Rectangle {
                width: index === view.currentIndex ? 22 : 8; height: 8; radius: 4
                color: index === view.currentIndex ? Theme.accent : Theme.sep
                Behavior on width { NumberAnimation { duration: 150 } }
            }
        }
    }
    PrimaryButton {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: Theme.safeBottom + 24 }
        width: Math.min(parent.width - 2 * 24, 420)
        text: view.currentIndex < page.slides.length - 1 ? qsTr("Continue") : qsTr("Get Started")
        onClicked: view.currentIndex < page.slides.length - 1 ? view.incrementCurrentIndex() : page.finished()
    }
}
