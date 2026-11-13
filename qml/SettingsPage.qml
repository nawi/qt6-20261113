import QtQuick

Rectangle {
    id: page
    signal done()
    color: Theme.bg

    Flickable {
        id: flick
        anchors { fill: parent; topMargin: nav.height }
        contentHeight: col.height + Theme.safeBottom + 48
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: contentHeight > height
        onMovementStarted: Qt.inputMethod.hide()

        Column {
            id: col
            x: (flick.width - width) / 2
            y: 12
            width: Math.min(flick.width - Theme.safeLeft - Theme.safeRight - 2 * Theme.margin, Theme.maxWidth)
            spacing: 24

            Group {
                header: qsTr("Appearance")
                Item {
                    width: parent.width; height: 56
                    property bool isFirst: true; property bool isLast: true
                    Segmented {
                        anchors { left: parent.left; right: parent.right; margins: Theme.margin; verticalCenter: parent.verticalCenter }
                        model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                        currentIndex: ["system", "light", "dark"].indexOf(appSettings.theme)
                        onActivated: function(i) { appSettings.theme = ["system", "light", "dark"][i] }
                    }
                }
            }

            Group {
                header: qsTr("Data")
                footer: qsTr("Rows per page in table data. SQL editor results are capped at the result limit. Queries stop after the timeout.")
                FieldCell { id: fPage; title: qsTr("Page size"); numeric: true; labelWidth: 170
                            text: appSettings.pageSize
                            onCommitted: { appSettings.pageSize = parseInt(text) || 100; text = appSettings.pageSize } }
                FieldCell { id: fRows; title: qsTr("Result limit"); numeric: true; labelWidth: 170
                            text: appSettings.rowLimit
                            onCommitted: { appSettings.rowLimit = parseInt(text) || 100; text = appSettings.rowLimit } }
                FieldCell { id: fTimeout; title: qsTr("Query timeout (s)"); numeric: true; labelWidth: 170
                            text: appSettings.queryTimeout
                            onCommitted: { appSettings.queryTimeout = parseInt(text) || 300; text = appSettings.queryTimeout } }
            }

            Group {
                header: qsTr("About")
                Cell { title: qsTr("Version"); value: buildInfo.version; tappable: false }
                Cell { title: qsTr("MariaDB Connector/C"); value: buildInfo.mariadb; tappable: false }
                Cell { title: qsTr("OpenSSL"); value: buildInfo.openssl.split(" ")[1]; tappable: false }
            }
        }
    }

    NavBar {
        id: nav
        title: qsTr("Settings")
        leading: NavButton { icon: "chevronLeft"; text: qsTr("Back"); onClicked: { Qt.inputMethod.hide(); page.done() } }
    }
}
