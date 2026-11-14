import QtQuick
import QtQuick.Controls
import NawiDBMan

Rectangle {
    id: page
    property int connId: 0
    property bool showingObjects: true
    readonly property bool wide: width >= 600
    signal done()

    color: Theme.bg

    Workspace {
        id: ws
        connId: page.connId
        store: connections
        tunnels: tunnelManager
        onTunnelPrompt: function(code, message, fingerprint) {
            trustSheet.code = code
            trustSheet.fp = fingerprint
            trustSheet.message = message + "\n\n" + fingerprint
            trustSheet.open()
        }
    }

    property var activeTab: {
        var n = ws.tabModel.count
        return ws.tabModel.tabAt(ws.currentIndex)
    }

    Component.onCompleted: {
        ws.start()
        var s = platform.debugStart
        if (s === "workspace_table") { ws.openTable("customers", "table"); page.showingObjects = false }
        else if (s === "workspace_design") { ws.openDesign("table", "customers"); page.showingObjects = false }
        else if (s === "workspace_query") {
            ws.openQuery(); page.showingObjects = false
            Qt.callLater(function() {
                var t = ws.tabModel.tabAt(0)
                if (t) { t.editorText = "SELECT id, name, balance FROM customers ORDER BY id"; t.run() }
            })
        }
    }

    NavBar {
        id: nav
        title: ws.name !== "" ? ws.name : qsTr("Connection")
        progress: 1
        leading: NavButton { icon: "chevronLeft"; text: qsTr("Back"); onClicked: page.done() }
        trailing: NavButton { icon: "plus"; onClicked: { ws.openQuery(); page.showingObjects = false } }
    }

    // connection strip
    Rectangle {
        id: strip
        anchors { left: parent.left; right: parent.right; top: nav.bottom }
        height: 36
        color: Theme.bg
        Column {
            x: Theme.margin + Theme.safeLeft
            anchors.verticalCenter: parent.verticalCenter
            Text { text: ws.detail; color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight
                   width: page.width - 2 * Theme.margin }
            Text { visible: ws.status !== ""; text: ws.status; color: ws.error !== "" ? Theme.danger : Theme.text2
                   font.pixelSize: 11; elide: Text.ElideRight; width: page.width - 2 * Theme.margin }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 0.5; color: Theme.sep }
    }

    // ---- object list ---------------------------------------------------------
    Rectangle {
        id: objectsPanel
        anchors { left: parent.left; top: strip.bottom; bottom: tabBar.top }
        width: page.wide ? 300 : page.width
        visible: page.wide || page.showingObjects
        color: Theme.bg
        clip: true

        Rectangle {
            id: searchBox
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
            height: 38
            radius: 10
            color: Theme.fill
            TextField {
                anchors.fill: parent
                anchors.margins: 4
                leftPadding: 10
                background: null
                color: Theme.text
                placeholderText: qsTr("Search objects")
                placeholderTextColor: Theme.text2
                font.pixelSize: 15
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onTextEdited: ws.setFilter(text)
            }
        }

        Rectangle {
            id: warnBox
            anchors { left: parent.left; right: parent.right; top: searchBox.bottom; margins: 12 }
            height: visible ? warnText.implicitHeight + 16 : 0
            visible: ws.warnings.length > 0
            radius: 10
            color: Qt.rgba(1, 0.6, 0.1, 0.12)
            Text {
                id: warnText
                anchors { fill: parent; margins: 8 }
                text: qsTr("Limited privileges: ") + ws.warnings.join(", ")
                color: Theme.text2; font.pixelSize: 11; wrapMode: Text.Wrap
            }
        }

        ListView {
            id: objectList
            anchors { left: parent.left; right: parent.right; top: warnBox.bottom; bottom: parent.bottom; margins: 12 }
            clip: true
            model: ws.objects
            section.property: "section"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                width: objectList.width; height: 30
                Text { anchors { left: parent.left; bottom: parent.bottom; bottomMargin: 4 }
                       text: section.toUpperCase(); color: Theme.text2
                       font { pixelSize: 11; weight: Font.DemiBold } }
            }
            delegate: Rectangle {
                width: objectList.width
                height: 46
                radius: 8
                color: ma.pressed ? Theme.pressed : "transparent"
                Row {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 4 }
                    spacing: 10
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: kind === "table" ? "table" : kind === "view" ? "globe"
                              : kind === "trigger" ? "link" : kind === "function" ? "shield" : "database"
                        color: Theme.accent; size: 18
                    }
                    Text {
                        width: parent.width - 60
                        anchors.verticalCenter: parent.verticalCenter
                        text: name; color: Theme.text; font.pixelSize: 15; elide: Text.ElideRight
                    }
                    Item {
                        width: 36; height: 36
                        anchors.verticalCenter: parent.verticalCenter
                        visible: kind === "table" || kind === "view"
                        Icon { anchors.centerIn: parent; name: "chevronRight"; color: Theme.text2; size: 15 }
                    }
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    onClicked: {
                        page.showingObjects = false
                        if (model.kind === "table" || model.kind === "view") ws.openTable(model.name, model.kind)
                        else ws.openDesign(model.kind, model.name)
                    }
                }
            }
        }

        Column {
            anchors.centerIn: parent
            visible: ws.objects.count === 0 && !ws.opening && ws.error === ""
            spacing: 8
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("No objects")
                   color: Theme.text2; font.pixelSize: 16 }
        }
    }

    // ---- tab content ---------------------------------------------------------
    Item {
        id: contentArea
        anchors { left: page.wide ? objectsPanel.right : parent.left; right: parent.right
                  top: strip.bottom; bottom: tabBar.top }
        visible: page.wide || !page.showingObjects
        clip: true

        Loader {
            id: contentLoader
            anchors.fill: parent
            sourceComponent: {
                var t = page.activeTab
                if (!t) return null
                return t.kind === "table" ? tableComp : t.kind === "query" ? queryComp : designComp
            }
            onLoaded: if (item) item.tab = page.activeTab
            Connections {
                target: page
                function onActiveTabChanged() {
                    if (contentLoader.item) contentLoader.item.tab = page.activeTab
                }
            }
        }

        Rectangle {
            visible: !page.activeTab && ws.opened
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 340)
            Column {
                width: parent.width
                spacing: 10
                Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "table"; size: 40; color: Theme.text2 }
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter
                       text: qsTr("Choose a table or run a query"); color: Theme.text2
                       font.pixelSize: 15; wrapMode: Text.Wrap }
                PrimaryButton { width: parent.width; text: qsTr("New SQL Tab")
                                onClicked: { ws.openQuery(); page.showingObjects = false } }
            }
        }

        // error / opening overlay
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 380)
            spacing: 12
            visible: ws.error !== ""
            Text { width: parent.width; text: ws.error; color: Theme.danger; font.pixelSize: 15
                   wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter }
            PrimaryButton { width: parent.width; prominent: false; text: qsTr("Retry"); onClicked: ws.retry() }
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: ws.opening && ws.error === ""
            visible: running
        }
    }

    // ---- tab bar -------------------------------------------------------------
    Rectangle {
        id: tabBar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 52 + Theme.safeBottom
        visible: ws.tabModel.count > 0
        color: Theme.card
        Rectangle { anchors.top: parent.top; width: parent.width; height: 0.5; color: Theme.sep }

        Row {
            anchors { fill: parent; bottomMargin: Theme.safeBottom; leftMargin: 6; rightMargin: 6 }
            NavButton {
                text: qsTr("Objects")
                color: page.showingObjects ? Theme.accent : Theme.text2
                onClicked: page.showingObjects = true
            }
            ListView {
                width: parent.width - 160
                height: parent.height
                orientation: ListView.Horizontal
                clip: true
                model: ws.tabModel
                currentIndex: ws.currentIndex
                delegate: Rectangle {
                    width: Math.min(160, Math.max(90, tabTitle.implicitWidth + 44))
                    height: parent.height - 8
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 9
                    color: index === ws.currentIndex ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                                                     : "transparent"
                    Text {
                        id: tabTitle
                        anchors { left: parent.left; leftMargin: 10; right: closeBtn.left; verticalCenter: parent.verticalCenter }
                        text: title; color: Theme.text; font { pixelSize: 13; weight: index === ws.currentIndex ? Font.DemiBold : Font.Normal }
                        elide: Text.ElideRight
                    }
                    Item {
                        id: closeBtn
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        width: 30; height: 30
                        Icon { anchors.centerIn: parent; name: "trash"; size: 13; color: Theme.text2 }
                        MouseArea { anchors.fill: parent; onClicked: ws.closeTab(index) }
                    }
                    MouseArea {
                        anchors { left: parent.left; right: closeBtn.left; top: parent.top; bottom: parent.bottom }
                        onClicked: { ws.currentIndex = index; page.showingObjects = false }
                    }
                }
            }
            NavButton { icon: "plus"; onClicked: { ws.openQuery(); page.showingObjects = false } }
        }
    }

    Component { id: tableComp
        TableTabView { connectionLabel: ws.detail } }
    Component { id: queryComp
        QueryTabView { connectionLabel: ws.detail } }
    Component { id: designComp
        DesignTabView { connectionLabel: ws.detail } }

    ActionSheet {
        id: trustSheet
        property int code: 0
        property string fp
        property string hostMessage
        title: qsTr("SSH host key")
        actions: code === 3 ? [{ text: qsTr("Trust and connect") }] : []
        onChosen: ws.trustTunnel(fp)
    }
}
