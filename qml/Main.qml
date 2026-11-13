import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: win
    visible: true
    width: 412; height: 892          // desktop preview only
    title: "NawiDBMan"
    color: Theme.bg

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
            onOpenRequested: function(id, name) { toast.show(qsTr("Opening \"%1\" arrives in the next release.").arg(name)) }
        } }
    Component { id: formPage
        ConnectionForm { onDone: stack.pop() } }
    Component { id: settingsPage
        SettingsPage { onDone: stack.pop() } }

    Toast { id: toast; y: win.height - height - Theme.safeBottom - 24; z: 10 }

    // Android back: pop instead of quitting
    onClosing: function(close) {
        if (stack.depth > 1) { close.accepted = false; stack.pop() }
    }

    // desktop preview helper: NAWI_START=form|formssh|settings
    Timer {
        interval: 50; running: platform.debugStart !== "" && platform.debugStart !== "onboarding"
        onTriggered: {
            var s = platform.debugStart
            if (s === "form") stack.push(formPage, { connId: 0 })
            else if (s === "formssh") stack.push(formPage, { connId: 2 })
            else if (s === "settings") stack.push(settingsPage)
        }
    }
}
