import QtQuick
import QtQuick.Controls

Rectangle {
    id: page
    color: Theme.bg
    signal addRequested()
    signal editRequested(int id)
    signal settingsRequested()
    signal openRequested(int id, string name)

    readonly property var modeNames: [qsTr("Direct"), qsTr("SSH"), qsTr("HTTP")]

    ListView {
        id: list
        anchors { fill: parent; topMargin: nav.height }
        model: connections
        clip: true
        spacing: 12
        leftMargin: Math.max(Theme.margin + Theme.safeLeft, (width - Theme.maxWidth) / 2)
        rightMargin: Math.max(Theme.margin + Theme.safeRight, (width - Theme.maxWidth) / 2)
        bottomMargin: Theme.safeBottom + 24
        boundsBehavior: Flickable.StopAtBounds

        header: Item {
            width: list.width - list.leftMargin - list.rightMargin; height: 56
            Text {
                anchors { left: parent.left; bottom: parent.bottom; bottomMargin: 8 }
                text: qsTr("Connections"); color: Theme.text; font { pixelSize: 34; weight: Font.Bold }
            }
        }

        delegate: Rectangle {
            id: card
            width: list.width - list.leftMargin - list.rightMargin
            height: body.height + 32
            radius: Theme.radius
            color: tap.pressed ? Theme.pressed : Theme.card

            Rectangle {   // label color stripe
                x: 0; y: 14; width: 5; height: parent.height - 28; radius: 2.5
                color: model.color
            }
            Column {
                id: body
                x: 20; y: 16
                width: parent.width - 20 - 56
                spacing: 4
                Row {
                    spacing: 8
                    Text { text: model.name; color: Theme.text; font { pixelSize: 19; weight: Font.DemiBold }
                           elide: Text.ElideRight; width: Math.min(implicitWidth, body.width - badge.width - 8) }
                    Rectangle {
                        id: badge
                        anchors.verticalCenter: parent.verticalCenter
                        width: bt.implicitWidth + 14; height: 20; radius: 10
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                        Text { id: bt; anchors.centerIn: parent; text: page.modeNames[model.mode]; color: Theme.accent
                               font { pixelSize: 12; weight: Font.DemiBold } }
                    }
                }
                Text { width: parent.width; elide: Text.ElideRight; color: Theme.text2; font.pixelSize: 15
                       text: model.mode === 2 ? model.httpUrl : (model.host + ":" + model.port) }
                Text { width: parent.width; elide: Text.ElideRight; color: Theme.text2; font.pixelSize: 15
                       text: model.user + " @ " + model.database + (model.ssl ? "  \u00B7  SSL" : "") }
                Text { visible: model.lastUsed > 0; color: Theme.text2; font.pixelSize: 13
                       text: qsTr("Last used %1").arg(new Date(model.lastUsed * 1000).toLocaleString(Qt.locale(), Locale.ShortFormat)) }
            }
            MouseArea { id: tap; anchors.fill: parent; onClicked: page.openRequested(model.id, model.name) }
            Item {   // edit button
                anchors { right: parent.right; top: parent.top }
                width: 56; height: 56
                opacity: em.pressed ? 0.5 : 1
                Icon { anchors.centerIn: parent; name: "pencil"; size: 20 }
                MouseArea { id: em; anchors.fill: parent; onClicked: page.editRequested(model.id) }
            }
        }

        footer: Item { width: 1; height: 1 }
    }

    // empty state
    Column {
        visible: connections.count === 0
        anchors { centerIn: parent; verticalCenterOffset: 20 }
        width: Math.min(parent.width - 64, 360)
        spacing: 14
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 88; height: 88; radius: 24
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
            Icon { anchors.centerIn: parent; name: "database"; size: 44; strokeWidth: 1.6 }
        }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("No connections yet")
               color: Theme.text; font { pixelSize: 22; weight: Font.DemiBold } }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
               text: qsTr("Add a MariaDB or MySQL server to get started."); color: Theme.text2; font.pixelSize: 16 }
        PrimaryButton { width: parent.width; text: qsTr("Add Connection"); onClicked: page.addRequested() }
    }

    NavBar {
        id: nav
        title: qsTr("Connections")
        progress: Math.max(0, Math.min(1, (list.contentY - list.originY) / 28))
        leading: NavButton { icon: "sliders"; onClicked: page.settingsRequested() }
        trailing: NavButton { icon: "plus"; onClicked: page.addRequested() }
    }
}
