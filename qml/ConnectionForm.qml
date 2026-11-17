import QtQuick
import QtQuick.Controls

Rectangle {
    id: page
    property int connId: 0
    signal done()
    color: Theme.bg

    property int mode: 0
    property int sshAuth: 0
    property bool ssl: false
    property bool httpEncrypt: true
    property string labelColor: Theme.labelColors[0]
    property string testMessage
    property bool testOk: false
    property string trustedFp

    function collect() {
        return {
            id: connId, name: fName.text, mode: mode, host: fHost.text, port: fPort.text, user: fUser.text,
            password: fPass.text, database: fDb.text, ssl: ssl, color: labelColor,
            sshHost: fSshHost.text, sshPort: fSshPort.text, sshUser: fSshUser.text, sshAuth: sshAuth,
            sshSecret: sshAuth === 0 ? fSshPass.text : keyArea.text, sshFp: trustedFp,
            httpUrl: fUrl.text, httpToken: fToken.text, httpEncrypt: httpEncrypt
        }
    }
    function load() {
        if (connId <= 0) return
        var c = connections.get(connId)
        fName.text = c.name; mode = c.mode; fHost.text = c.host; fPort.text = c.port; fUser.text = c.user
        fPass.text = c.password; fDb.text = c.database; ssl = c.ssl === 1 || c.ssl === true; labelColor = c.color
        fSshHost.text = c.sshHost; fSshPort.text = c.sshPort; fSshUser.text = c.sshUser; sshAuth = c.sshAuth
        if (sshAuth === 0) fSshPass.text = c.sshSecret; else keyArea.text = c.sshSecret
        trustedFp = c.sshFp || ""
        fUrl.text = c.httpUrl; fToken.text = c.httpToken
        httpEncrypt = c.httpEncrypt === undefined ? true : (c.httpEncrypt === 1 || c.httpEncrypt === true)
    }
    Component.onCompleted: load()

    Connections {
        target: connections
        function onTestFinished(ok, message) { page.testOk = ok; page.testMessage = message }
        function onHostKeyNeeded(fingerprint, changed) {
            trustSheet.fingerprint = fingerprint
            trustSheet.changed = changed
            trustSheet.open()
        }
    }

    FormFlickable {
        id: flick
        anchors { fill: parent; topMargin: nav.height }

        Column {
            id: col
            x: (flick.width - width) / 2
            y: 12
            width: Math.min(flick.width - Theme.safeLeft - Theme.safeRight - 2 * Theme.margin, Theme.maxWidth)
            spacing: 24

            Group {
                header: qsTr("General")
                FieldCell { id: fName; title: qsTr("Name"); placeholder: qsTr("My Server") }
                Item {
                    width: parent.width; height: 52
                    property bool isFirst: false
                    property bool isLast: true
                    Rectangle { anchors { top: parent.top; right: parent.right } width: parent.width - Theme.margin; height: 0.5; color: Theme.sep }
                    Text { x: Theme.margin; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Color"); color: Theme.text; font.pixelSize: 17 }
                    Row {
                        anchors { right: parent.right; rightMargin: Theme.margin - 4; verticalCenter: parent.verticalCenter }
                        spacing: 4
                        Repeater {
                            model: Theme.labelColors
                            delegate: Item {
                                width: 30; height: 44
                                Rectangle {
                                    anchors.centerIn: parent; width: 22; height: 22; radius: 11; color: modelData
                                    Rectangle { anchors.centerIn: parent; width: 30; height: 30; radius: 15; color: "transparent"
                                                border { width: 2; color: modelData } visible: page.labelColor === modelData }
                                }
                                MouseArea { anchors.fill: parent; onClicked: page.labelColor = modelData }
                            }
                        }
                    }
                }
            }

            Segmented {
                width: parent.width
                model: [qsTr("Direct"), qsTr("SSH Tunnel"), qsTr("HTTP Tunnel")]
                currentIndex: page.mode
                onActivated: function(i) { page.mode = i; page.testMessage = "" }
            }

            Group {
                header: qsTr("Database Server")
                visible: page.mode !== 2
                height: visible ? implicitHeight : 0
                FieldCell { id: fHost; title: qsTr("Host"); placeholder: "db.example.com"; visible: page.mode !== 2 }
                FieldCell { id: fPort; title: qsTr("Port"); numeric: true; text: "3306"; visible: page.mode !== 2 }
                FieldCell { id: fUser; title: qsTr("Username"); placeholder: "root" }
                FieldCell { id: fPass; title: qsTr("Password"); secret: true; placeholder: qsTr("Optional") }
                FieldCell { id: fDb; title: qsTr("Database"); placeholder: qsTr("Required") }
                Cell {
                    title: qsTr("Use SSL"); tappable: false
                    IOSSwitch { checked: page.ssl; onToggled: page.ssl = checked }
                }
            }

            Group {
                header: qsTr("Database Login")
                visible: page.mode === 2
                height: visible ? implicitHeight : 0
                footer: qsTr("The agent connects to the database from the remote server, so use the host as seen from that server (usually localhost).")
                FieldCell { title: qsTr("Host"); placeholder: "localhost"; text: fHost.text; visible: page.mode === 2; onEdited: fHost.text = text }
                FieldCell { title: qsTr("Port"); numeric: true; text: fPort.text; visible: page.mode === 2; onEdited: fPort.text = text }
            }

            Group {
                header: qsTr("SSH Tunnel")
                visible: page.mode === 1
                height: visible ? implicitHeight : 0
                footer: qsTr("Host key is verified on first connect and remembered. Private keys must not use a passphrase.")
                FieldCell { id: fSshHost; title: qsTr("SSH Host"); placeholder: "ssh.example.com"; visible: page.mode === 1 }
                FieldCell { id: fSshPort; title: qsTr("SSH Port"); numeric: true; text: "22"; visible: page.mode === 1 }
                FieldCell { id: fSshUser; title: qsTr("SSH User"); visible: page.mode === 1 }
                Item {
                    width: parent.width; height: visible ? 52 : 0; visible: page.mode === 1
                    property bool isFirst: false; property bool isLast: false
                    Rectangle { anchors { top: parent.top; right: parent.right } width: parent.width - Theme.margin; height: 0.5; color: Theme.sep }
                    Segmented {
                        anchors { left: parent.left; right: parent.right; margins: Theme.margin; verticalCenter: parent.verticalCenter }
                        model: [qsTr("Password"), qsTr("Private Key")]
                        currentIndex: page.sshAuth
                        onActivated: function(i) { page.sshAuth = i }
                    }
                }
                FieldCell { id: fSshPass; title: qsTr("Password"); secret: true; visible: page.mode === 1 && page.sshAuth === 0 }
                Item {
                    visible: page.mode === 1 && page.sshAuth === 1
                    width: parent.width; height: visible ? 150 : 0
                    property bool isFirst: false; property bool isLast: true
                    clip: true
                    Rectangle { anchors { top: parent.top; right: parent.right } width: parent.width - Theme.margin; height: 0.5; color: Theme.sep }
                    Flickable {
                        id: keyFlick
                        anchors { fill: parent; margins: 8 }
                        clip: true
                        flickableDirection: Flickable.VerticalFlick
                        ScrollBar.vertical: ScrollBar {}
                        TextArea.flickable: TextArea {
                            id: keyArea
                            width: keyFlick.width
                            background: null
                            color: Theme.text
                            placeholderText: qsTr("Paste private key (-----BEGIN ... PRIVATE KEY-----)")
                            placeholderTextColor: Theme.text2
                            font { pixelSize: 13; family: "monospace" }
                            wrapMode: TextArea.WrapAnywhere
                            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                            selectByMouse: true
                        }
                    }
                }
            }

            Group {
                header: qsTr("PHP Tunnel Agent")
                visible: page.mode === 2
                height: visible ? implicitHeight : 0
                FieldCell { id: fUrl; title: qsTr("Agent URL"); placeholder: "https://example.com/tunnel.php"; visible: page.mode === 2 }
                FieldCell { id: fToken; title: qsTr("Token"); secret: true; visible: page.mode === 2 }
                Cell {
                    title: qsTr("Encrypt requests")
                    value: page.httpEncrypt ? qsTr("On") : qsTr("Off")
                    tappable: false
                    IOSSwitch { checked: page.httpEncrypt; onToggled: page.httpEncrypt = checked }
                }
            }

            Group {
                visible: page.mode === 2
                height: visible ? implicitHeight : 0
                FieldCell { title: qsTr("Username"); text: fUser.text; visible: page.mode === 2; onEdited: fUser.text = text }
                FieldCell { title: qsTr("Password"); secret: true; text: fPass.text; visible: page.mode === 2; onEdited: fPass.text = text }
                FieldCell { title: qsTr("Database"); placeholder: qsTr("Required"); text: fDb.text; visible: page.mode === 2; onEdited: fDb.text = text }
            }

            Column {
                width: parent.width
                spacing: 12
                PrimaryButton {
                    width: parent.width
                    prominent: false
                    text: qsTr("Test Connection")
                    busy: connections.testing
                    onClicked: { Qt.inputMethod.hide(); page.testMessage = ""; connections.testConnection(page.collect()) }
                }
                Text {
                    visible: page.testMessage !== ""
                    width: parent.width; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                    text: page.testMessage
                    color: page.testOk ? Theme.success : Theme.danger
                    font.pixelSize: 15
                }
            }

            PrimaryButton {
                visible: page.connId > 0
                width: parent.width
                prominent: false
                tint: Theme.danger
                text: qsTr("Delete Connection")
                onClicked: confirm.open()
            }
        }
    }

    NavBar {
        id: nav
        title: page.connId > 0 ? qsTr("Edit Connection") : qsTr("New Connection")
        progress: 1
        leading: NavButton { icon: "chevronLeft"; text: qsTr("Back"); onClicked: page.done() }
        trailing: NavButton {
            text: qsTr("Save"); bold: true
            onClicked: {
                var e = connections.save(page.collect())
                if (e === "") page.done(); else { page.testOk = false; page.testMessage = e; flick.contentY = 0 }
            }
        }
    }

    ActionSheet {
        id: trustSheet
        property string fingerprint
        property bool changed: false
        title: changed ? qsTr("SSH host key changed") : qsTr("Unknown SSH host key")
        message: changed
                 ? qsTr("The server key no longer matches the saved one. Connection refused.\n\nSHA256 %1").arg(fingerprint)
                 : qsTr("Verify this fingerprint with your server administrator.\n\nSHA256 %1").arg(fingerprint)
        actions: changed ? [] : [{ text: qsTr("Trust and test") }]
        onChosen: {
            page.trustedFp = fingerprint
            connections.testConnection(page.collect())
        }
    }

    ActionSheet {
        id: confirm
        title: qsTr("Delete this connection?")
        message: qsTr("Saved credentials for it will be removed from this device.")
        actions: [{ text: qsTr("Delete"), destructive: true }]
        onChosen: { connections.remove(page.connId); page.done() }
    }
}
