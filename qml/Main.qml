import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: win
    visible: true
    width: platform.debugStart === "wide" ? 960 : 412   // desktop preview only
    height: platform.debugStart === "wide" ? 760 : 892
    title: "NawiDBMan"
    color: Theme.bg

    // Qt >= 6.9 pads contentItem by the safe area automatically. We handle insets ourselves
    // (Theme.safe*, incl. keyboard), so disable it to avoid double insets.
    topPadding: 0; bottomPadding: 0; leftPadding: 0; rightPadding: 0

    Component.onCompleted: { Theme.settings = appSettings; Theme.platform = platform }
    Connections {
        target: Theme
        function onDarkChanged() { platform.setLightBars(!Theme.dark) }
    }
    onVisibleChanged: if (visible) platform.setLightBars(!Theme.dark)

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: (appSettings.onboarded && platform.debugStart !== "onboarding") ? listPage : onboardingPage

        pushEnter: Transition { XAnimator { from: stack.width; to: 0; duration: 280; easing.type: Easing.OutCubic } }
        pushExit: Transition { XAnimator { from: 0; to: -stack.width * 0.3; duration: 280; easing.type: Easing.OutCubic } }
        popEnter: Transition { XAnimator { from: -stack.width * 0.3; to: 0; duration: 280; easing.type: Easing.OutCubic } }
        popExit: Transition { XAnimator { from: 0; to: stack.width; duration: 280; easing.type: Easing.OutCubic } }
        replaceEnter: Transition { OpacityAnimator { from: 0; to: 1; duration: 250 } }
        replaceExit: Transition { OpacityAnimator { from: 1; to: 0; duration: 250 } }
    }

    Component { id: onboardingPage
        Onboarding { onFinished: { appSettings.onboarded = true; stack.replace(listPage) } } }
    Component { id: listPage
        ConnectionList {
            onAddRequested: stack.push(formPage, { connId: 0 })
            onEditRequested: function(id) { stack.push(formPage, { connId: id }) }
            onSettingsRequested: stack.push(settingsPage)
            onOpenRequested: function(id, name) { stack.push(workspacePage, { connId: id }) }
        } }
    Component { id: formPage
        ConnectionForm { onDone: stack.pop() } }
    Component { id: settingsPage
        SettingsPage { onDone: stack.pop() } }
    Component { id: workspacePage
        WorkspacePage { onDone: stack.pop() } }

    Toast { id: toast; y: win.height - height - Theme.safeBottom - 24; z: 10 }

    // Android Back (Qt >= 6.12 no longer sends Window.onClosing; it moves the task to background).
    // Window-level Shortcut fires regardless of focus; "Back" (not StandardKey.Back) avoids Backspace/Alt+Left.
    Shortcut {
        sequences: ["Back"]
        enabled: Qt.platform.os === "android" && stack.depth > 1
        onActivated: stack.pop()
    }

    // desktop preview helper: NAWI_START=form|formssh|settings
    Timer {
        interval: 50; repeat: false; running: platform.debugStart !== "" && platform.debugStart !== "onboarding"
        onTriggered: {
            var s = platform.debugStart
            if (s === "form") stack.push(formPage, { connId: 0 })
            else if (s === "formssh") stack.push(formPage, { connId: 2 })
            else if (s === "settings") stack.push(settingsPage)
            else if (s === "wide" && connections.count > 0)
                stack.push(workspacePage, { connId: connections.firstId() })
            else if (s.startsWith("workspace") && connections.count > 0)
                stack.push(workspacePage, {
                    connId: s.indexOf("ssh") >= 0 ? connections.lastId() : connections.firstId()
                })
        }
    }
}
