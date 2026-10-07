import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CodeQuizzio

ApplicationWindow {
    id: win
    visible: true
    width: 960
    height: 640
    minimumWidth: 640
    minimumHeight: 480
    title: "CodeQuizzio"
    color: theme.bg

    QuizController { id: quiz }

    QtObject {
        id: theme
        readonly property color bg: "#14161f"
        readonly property color card: "#1e2230"
        readonly property color cardBorder: "#2b3046"
        readonly property color text: "#e8ebf5"
        readonly property color muted: "#8a90a8"
        readonly property color accent: "#6c8cff"
        readonly property color good: "#3ddc97"
        readonly property color warn: "#ffc857"
        readonly property color bad: "#ff5d73"
        readonly property string mono: "Consolas"
    }

    readonly property bool asking: quiz.state === QuizController.Asking
    readonly property bool finished: quiz.state === QuizController.Finished
    readonly property bool skipped: quiz.state === QuizController.Skipped

    // Esc pauses/resumes, Ctrl+S skips, Enter leaves the skip review card.
    Shortcut { sequence: "Esc"; enabled: win.asking; onActivated: quiz.togglePause() }
    Shortcut { sequence: "Ctrl+S"; enabled: win.asking && !quiz.paused; onActivated: quiz.skip() }
    Shortcut { sequences: ["Return", "Enter"]; enabled: win.skipped; onActivated: quiz.submit("") }

    component Stat: ColumnLayout {
        property string label
        property string value
        property color valueColor: theme.text
        spacing: 0
        Text { text: parent.label; color: theme.muted; font.pixelSize: 12; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
        Text { text: parent.value; color: parent.valueColor; font.pixelSize: 28; font.bold: true }
    }

    component PillButton: Button {
        id: pill
        property color tint: theme.accent
        implicitHeight: 40
        implicitWidth: Math.max(110, label.implicitWidth + 36)
        opacity: enabled ? 1.0 : 0.35
        contentItem: Text {
            id: label
            text: pill.text
            color: theme.text
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 20
            color: pill.down ? Qt.darker(pill.tint, 1.4) : pill.hovered ? Qt.darker(pill.tint, 1.15) : "transparent"
            border.color: pill.tint
            border.width: 1.5
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 24

        // ---- Header: title + live stats ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 32
            Text {
                text: "Code<font color='" + theme.accent + "'>Quizzio</font>"
                textFormat: Text.StyledText
                color: theme.text
                font.pixelSize: 26
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Stat { label: "Score"; value: quiz.score }
            Stat {
                label: "Streak"
                value: quiz.streak
                valueColor: quiz.streak >= 5 ? theme.warn : theme.text
            }
        }

        // ---- Main area ----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Load problem (missing/bad question files).
            Text {
                anchors.centerIn: parent
                visible: quiz.loadError !== ""
                width: parent.width * 0.8
                text: quiz.loadError
                color: theme.bad
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 16
            }

            // Quiz card
            Rectangle {
                id: card
                anchors.fill: parent
                visible: !win.finished
                radius: 20
                color: theme.card
                border.width: 2
                border.color: flash.running ? theme.bad
                    : quiz.state === QuizController.Correct ? theme.good
                    : win.skipped ? theme.warn
                    : theme.cardBorder

                SequentialAnimation {
                    id: flash
                    PropertyAnimation { target: card; property: "anchors.leftMargin"; to: 14; duration: 50 }
                    PropertyAnimation { target: card; property: "anchors.leftMargin"; to: -14; duration: 80 }
                    PropertyAnimation { target: card; property: "anchors.leftMargin"; to: 0; duration: 50 }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 32
                    spacing: 20

                    // Difficulty pips
                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: 10
                            Rectangle {
                                required property int index
                                width: 18; height: 6; radius: 3
                                color: index < quiz.difficulty ? theme.accent : theme.cardBorder
                            }
                        }
                        Text { text: "  Level " + quiz.difficulty; color: theme.muted; font.pixelSize: 13 }
                        Item { Layout.fillWidth: true }
                        // Marks the skip review card so it can't be mistaken for a live question.
                        Rectangle {
                            visible: win.skipped
                            radius: 10
                            color: "transparent"
                            border.color: theme.warn
                            border.width: 1.5
                            implicitWidth: skippedBadge.implicitWidth + 24
                            implicitHeight: 26
                            Text {
                                id: skippedBadge
                                anchors.centerIn: parent
                                text: "SKIPPED  ·  +0"
                                color: theme.warn
                                font.pixelSize: 12
                                font.bold: true
                                font.letterSpacing: 1
                            }
                        }
                    }

                    // Prompt (hidden while paused so you can't study it for free)
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Text {
                            anchors.fill: parent
                            visible: !quiz.paused
                            text: quiz.prompt
                            color: theme.text
                            font.pixelSize: 30
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: quiz.paused
                            text: "Paused\nPress Esc to resume"
                            color: theme.muted
                            font.pixelSize: 26
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Time bar: drains toward par, then the label goes red.
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        visible: win.asking
                        Rectangle {
                            Layout.fillWidth: true
                            height: 8; radius: 4
                            color: theme.cardBorder
                            Rectangle {
                                readonly property real remaining: quiz.par > 0 ? Math.max(0, 1 - quiz.elapsed / quiz.par) : 0
                                height: parent.height; radius: 4
                                width: parent.width * remaining
                                color: remaining > 0.3 ? theme.good : remaining > 0.12 ? theme.warn : theme.bad
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: quiz.elapsed.toFixed(1) + "s"; color: theme.muted; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true }
                            Text {
                                readonly property bool over: quiz.elapsed > quiz.par
                                text: over ? "+" + (quiz.elapsed - quiz.par).toFixed(1) + "s over par" : "par " + quiz.par.toFixed(1) + "s"
                                color: over ? theme.bad : theme.muted
                                font.pixelSize: 13
                            }
                        }
                    }

                    // Feedback line (correct / missed). A skip gets its own review block below.
                    Text {
                        Layout.fillWidth: true
                        visible: quiz.state === QuizController.Correct || quiz.state === QuizController.Missed
                        textFormat: Text.StyledText
                        wrapMode: Text.WordWrap
                        font.pixelSize: 18
                        color: quiz.state === QuizController.Correct ? theme.good : theme.bad
                        text: {
                            if (quiz.state === QuizController.Correct)
                                return "Correct!  +" + quiz.lastPoints + "  <font color='" + theme.muted + "'>speed x"
                                    + quiz.lastSpeedMultiplier.toFixed(2) + " · streak x" + quiz.lastStreakMultiplier.toFixed(2) + "</font>"
                            return "Not quite. Type the answer to continue:  <font face='" + theme.mono + "' color='" + theme.text + "'>"
                                + quiz.answerHint.replace(/&/g, "&amp;").replace(/</g, "&lt;") + "</font>"
                        }
                    }

                    // Skip review: the question above, the answer here, then Enter for the next one.
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: win.skipped
                        spacing: 8
                        Text {
                            text: "ANSWER"
                            color: theme.muted
                            font.pixelSize: 12
                            font.letterSpacing: 1
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: answerText.implicitHeight + 28
                            radius: 12
                            color: theme.bg
                            border.color: theme.warn
                            border.width: 2
                            Text {
                                id: answerText
                                anchors.fill: parent
                                anchors.margins: 14
                                text: quiz.answerHint
                                color: theme.text
                                font.family: theme.mono
                                font.pixelSize: 22
                                wrapMode: Text.WrapAnywhere
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Press Enter for the next question"; color: theme.muted; font.pixelSize: 14 }
                            Item { Layout.fillWidth: true }
                            PillButton { text: "Continue  (Enter)"; tint: theme.warn; onClicked: quiz.submit("") }
                        }
                    }

                    // Answer input (not shown on the skip review card)
                    TextField {
                        id: input
                        Layout.fillWidth: true
                        visible: !quiz.paused && !win.skipped
                        enabled: quiz.state !== QuizController.Correct
                        font.family: theme.mono
                        font.pixelSize: 22
                        color: theme.text
                        placeholderText: "Type your answer and press Enter"
                        placeholderTextColor: theme.muted
                        selectByMouse: true
                        padding: 14
                        background: Rectangle {
                            radius: 12
                            color: theme.bg
                            border.width: 2
                            border.color: input.activeFocus ? theme.accent : theme.cardBorder
                        }
                        onAccepted: {
                            quiz.submit(text)
                            text = ""
                        }
                    }
                }
            }

            // Session summary
            Rectangle {
                anchors.fill: parent
                visible: win.finished && quiz.loadError === ""
                radius: 20
                color: theme.card
                border.width: 2
                border.color: theme.cardBorder

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 24
                    Text { Layout.alignment: Qt.AlignHCenter; text: "Session over"; color: theme.text; font.pixelSize: 34; font.bold: true }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 48
                        Stat { label: "Score"; value: quiz.score; valueColor: theme.accent }
                        Stat { label: "Best streak"; value: quiz.bestStreak }
                        Stat {
                            label: "Accuracy"
                            value: {
                                var total = quiz.correctCount + quiz.wrongCount
                                return total === 0 ? "-" : Math.round(100 * quiz.correctCount / total) + "%"
                            }
                        }
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        color: theme.muted
                        font.pixelSize: 15
                        text: quiz.correctCount + " correct · " + quiz.wrongCount + " wrong · " + quiz.skippedCount + " skipped"
                    }
                    PillButton { Layout.alignment: Qt.AlignHCenter; text: "Play again"; onClicked: quiz.restart() }
                }
            }
        }

        // ---- Footer controls ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: !win.finished
            PillButton {
                text: quiz.paused ? "Resume  (Esc)" : "Pause  (Esc)"
                enabled: win.asking
                onClicked: quiz.togglePause()
            }
            PillButton {
                text: "Skip  (Ctrl+S)"
                tint: theme.warn
                enabled: win.asking && !quiz.paused
                onClicked: quiz.skip()
            }
            Item { Layout.fillWidth: true }
            PillButton { text: "End session"; tint: theme.bad; onClicked: quiz.endSession() }
        }
    }

    // Keep the cursor in the answer box and clear it when the screen changes.
    function refocus() {
        input.text = ""
        if (!quiz.paused && !win.finished) input.forceActiveFocus()
    }
    Connections {
        target: quiz
        function onStateChanged() { win.refocus() }
        function onPausedChanged() { win.refocus() }
        function onRetypeRejected() { flash.restart() }
    }
    Component.onCompleted: refocus()
}
