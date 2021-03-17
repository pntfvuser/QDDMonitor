import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import org.anon.QDDMonitor 1.0

StackView {
    id: stackviewViewDelegate

    clip: true

    Component {
        id: componentViewSettings

        Page {
            property bool audioPositionEnabled: false
            property vector3d audioPosition: Qt.vector3d(0, 0, 0)

            id: pageViewSettings

            header: ToolBar {
                contentHeight: buttonBack.implicitHeight

                ToolButton {
                    id: buttonBack

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter

                    icon.source: "images/arrow_back-black.svg"
                    font.pixelSize: Qt.application.font.pixelSize * 1.6

                    onClicked: {
                        if (stackviewViewDelegate.depth > 1) {
                            stackviewViewDelegate.pop();
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent

                    text: qsTr("Settings")
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 40

                GridLayout {
                    columns: 2
                    rowSpacing: 20
                    columnSpacing: 20

                    Label {
                        text: qsTr("3D Audio:")
                    }
                    CheckBox {
                        id: checkboxAudioPosition
                        Layout.fillWidth: true
                        text: qsTr("Enabled")
                    }

                    Item {
                        implicitWidth: 1
                        implicitHeight: 1
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("x:")
                        }
                        TextField {
                            id: textInputSourceX
                            Layout.fillWidth: true
                            enabled: pageViewSettings.audioPositionEnabled
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator {}
                            selectByMouse: true
                            text: pageViewSettings.audioPosition.x.toFixed(3)

                            onActiveFocusChanged: {
                                if (activeFocus) {
                                    text = pageViewSettings.audioPosition.x.toFixed(3);
                                } else {
                                    pageViewSettings.audioPosition.x = text;
                                    text = Qt.binding(function() { return pageViewSettings.audioPosition.x.toFixed(3); });
                                }
                            }
                        }

                        Label {
                            text: qsTr("y:")
                        }
                        TextField {
                            id: textInputSourceY
                            Layout.fillWidth: true
                            enabled: pageViewSettings.audioPositionEnabled
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator {}
                            selectByMouse: true
                            text: pageViewSettings.audioPosition.y.toFixed(3)

                            onActiveFocusChanged: {
                                if (activeFocus) {
                                    text = pageViewSettings.audioPosition.y.toFixed(3);
                                } else {
                                    pageViewSettings.audioPosition.y = text;
                                    text = Qt.binding(function() { return pageViewSettings.audioPosition.y.toFixed(3); });
                                }
                            }
                        }

                        Label {
                            text: qsTr("z:")
                        }
                        TextField {
                            id: textInputSourceZ
                            Layout.fillWidth: true
                            enabled: pageViewSettings.audioPositionEnabled
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator {}
                            selectByMouse: true
                            text: pageViewSettings.audioPosition.z.toFixed(3)

                            onActiveFocusChanged: {
                                if (activeFocus) {
                                    text = pageViewSettings.audioPosition.z.toFixed(3);
                                } else {
                                    pageViewSettings.audioPosition.z = text;
                                    text = Qt.binding(function() { return pageViewSettings.audioPosition.z.toFixed(3); });
                                }
                            }
                        }
                    }

                    Item {
                        implicitWidth: 1
                        implicitHeight: 1
                    }
                    Item {
                        property real ratio: Math.min(width - imageAudioSource.width, height - imageAudioSource.height) * 0.5

                        id: itemAudioSourcePosition

                        Layout.preferredWidth: 300
                        Layout.preferredHeight: 200

                        enabled: pageViewSettings.audioPositionEnabled

                        Rectangle {
                            anchors.fill: parent
                            border.color: "black"
                            border.width: 1
                        }

                        Image {
                            anchors.centerIn: parent
                            source: "images/hearing.svg"
                        }

                        Image {
                            id: imageAudioSource

                            x: pageViewSettings.audioPosition.x * parent.ratio - (width - parent.width) * 0.5
                            y: pageViewSettings.audioPosition.y * parent.ratio - (height - parent.height) * 0.5
                            source: "images/volume_up.svg"

                            Drag.active: mouseareaPositionDrag.drag.active
                            Drag.hotSpot.x: width * 0.5
                            Drag.hotSpot.y: height * 0.5

                            MouseArea {
                                id: mouseareaPositionDrag

                                anchors.fill: parent

                                preventStealing: true

                                drag.target: imageAudioSource
                                drag.smoothed: false
                                drag.minimumX: 0
                                drag.minimumY: 0
                                drag.maximumX: itemAudioSourcePosition.width - imageAudioSource.width
                                drag.maximumY: itemAudioSourcePosition.height - imageAudioSource.height

                                onPressed: {
                                    imageAudioSource.x = pageViewSettings.audioPosition.x * itemAudioSourcePosition.ratio - (imageAudioSource.width - itemAudioSourcePosition.width) * 0.5;
                                    imageAudioSource.y = pageViewSettings.audioPosition.y * itemAudioSourcePosition.ratio - (imageAudioSource.height - itemAudioSourcePosition.height) * 0.5;
                                    pageViewSettings.audioPosition.x = Qt.binding(function() { return (imageAudioSource.x + (imageAudioSource.width - itemAudioSourcePosition.width) * 0.5) / itemAudioSourcePosition.ratio; });
                                    pageViewSettings.audioPosition.y = Qt.binding(function() { return (imageAudioSource.y + (imageAudioSource.height - itemAudioSourcePosition.height) * 0.5) / itemAudioSourcePosition.ratio; });
                                }
                                onReleased: {
                                    pageViewSettings.audioPosition.x = (imageAudioSource.x + (imageAudioSource.width - itemAudioSourcePosition.width) * 0.5) / itemAudioSourcePosition.ratio;
                                    pageViewSettings.audioPosition.y = (imageAudioSource.y + (imageAudioSource.height - itemAudioSourcePosition.height) * 0.5) / itemAudioSourcePosition.ratio;
                                    imageAudioSource.x = Qt.binding(function() { return pageViewSettings.audioPosition.x * itemAudioSourcePosition.ratio - (imageAudioSource.width - itemAudioSourcePosition.width) * 0.5; });
                                    imageAudioSource.y = Qt.binding(function() { return pageViewSettings.audioPosition.y * itemAudioSourcePosition.ratio - (imageAudioSource.height - itemAudioSourcePosition.height) * 0.5; });
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            onAudioPositionEnabledChanged: {
                if (!audioPositionEnabled) {
                    audioPosition = Qt.vector3d(0, 0, 0);
                }
            }

            Component.onCompleted: {
                audioPositionEnabled = (liveview.position != Qt.vector3d(0, 0, 0));
                audioPosition = liveview.position;
                checkboxAudioPosition.checked = audioPositionEnabled;

                audioPositionEnabled = Qt.binding(function() { return checkboxAudioPosition.checked; });
                liveview.position = Qt.binding(function() { return audioPositionEnabled ? audioPosition : Qt.vector3d(0, 0, 0); });
            }
        }
    }

    initialItem: Page {
        Item {
            anchors.fill: parent

            Rectangle {
                anchors.fill: parent

                color: "transparent"
                border.color: "black"
                border.width: 1
            }

            LiveStreamView {
                id: liveview
                anchors.fill: parent

                source: display.sourceInfo ? display.sourceInfo.source : null
                audioOut: display.audioOut
                subtitleOut.textDelegate: Text {
                    text: subtitleText
                    color: subtitleColor
                    font.pixelSize: 24
                    font.bold: true
                    style: Text.Outline
                    styleColor: "black"
                }

                volume: sliderVolume.position

                t: playbackTimer
            }

            Rectangle {
                id: rectangleToolbar

                height: buttonViewSettings.implicitHeight
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right

                enabled: mouseareaView.containsMouse
                visible: mouseareaView.containsMouse

                color: "#55000000"

                RowLayout {
                    anchors.fill: parent
                    spacing: buttonViewSettings.implicitHeight * 0.25

                    Item {
                        Layout.fillWidth: true
                    }

                    Slider {
                        id: sliderVolume

                        Layout.preferredHeight: buttonViewSettings.implicitHeight * 0.5
                        Layout.preferredWidth: buttonViewSettings.implicitHeight * 2
                        Layout.alignment: Qt.AlignCenter

                        value: 0.5
                    }

                    Rectangle {
                        Layout.preferredHeight: buttonViewSettings.implicitHeight * 0.5
                        Layout.preferredWidth: buttonViewSettings.implicitHeight * 0.5

                        border.color: "black"
                        border.width: 1
                        color: liveview.mute ? "lightgreen" : "lightgray";

                        Text {
                            anchors.centerIn: parent
                            font.pixelSize: parent.height - 4
                            text: "M"
                        }

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                liveview.mute = !liveview.mute;
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: buttonViewSettings.implicitHeight * 0.5
                        Layout.preferredWidth: buttonViewSettings.implicitHeight * 0.5

                        border.color: "black"
                        border.width: 1
                        color: liveview.solo ? "gold" : "lightgray";

                        Text {
                            anchors.centerIn: parent
                            font.pixelSize: parent.height - 4
                            text: "S"
                        }

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                liveview.solo = !liveview.solo;
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: buttonViewSettings.implicitHeight * 0.5
                        Layout.preferredWidth: buttonViewSettings.implicitHeight * 0.5

                        border.color: "black"
                        border.width: 1
                        color: display.sourceInfo && display.sourceInfo.recording ? "red" : "lightgray";

                        Text {
                            anchors.centerIn: parent
                            font.pixelSize: parent.height - 4
                            text: "R"
                        }

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                if (display.sourceId !== -1)
                                    sourceModelMain.setSourceRecording(display.sourceId, !display.sourceInfo.recording);
                            }
                        }
                    }

                    ToolButton {
                        id: buttonViewSettings

                        icon.source: "images/settings-black.svg"
                        font.pixelSize: Qt.application.font.pixelSize * 1.6

                        onClicked: {
                            stackviewViewDelegate.push(componentViewSettings);
                        }
                    }
                }
            }

            MouseArea {
                id: mouseareaView

                anchors.fill: parent

                hoverEnabled: true
                preventStealing: true
                propagateComposedEvents: true

                onPressed: {
                    if (rectangleToolbar.contains(mapToItem(rectangleToolbar, mouse.x, mouse.y))) {
                        mouse.accepted = false;
                        return;
                    }
                    sourcePressed(display.sourceId, index);
                }

                onReleased: {
                    sourceReleased(mapToItem(null, mouse.x, mouse.y));
                }
            }

            Connections {
                target: windowMain

                function onMutePressed() {
                    if (mouseareaView.containsMouse) {
                        liveview.mute = !liveview.mute;
                    }
                }
                function onSoloPressed() {
                    if (mouseareaView.containsMouse) {
                        liveview.solo = !liveview.solo;
                    }
                }
                function onRecordPressed() {
                    if (mouseareaView.containsMouse) {
                        if (display.sourceId !== -1)
                            sourceModelMain.setSourceRecording(display.sourceId, !display.sourceInfo.recording);
                    }
                }
            }
        }
    }
}
