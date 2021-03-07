import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.anon.QDDMonitor 1.0

StackView {
    id: stackviewViewDelegate

    clip: true

    Component {
        id: componentViewSettings

        Page {
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
                            liveview.position = Qt.vector3d(textInputSourceX.text, textInputSourceY.text, textInputSourceZ.text);
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
                        text: qsTr("3D Audio: ")
                    }
                    CheckBox {
                        id: checkboxInputName
                        Layout.fillWidth: true
                        text: qsTr("Enabled")

                        checked: liveview.position != Qt.vector3d(0, 0, 0)

                        onCheckedChanged: {
                            if (!checked) {
                                textInputSourceX.text = textInputSourceY.text = textInputSourceZ.text = "0";
                            }
                        }
                    }

                    Item {
                        implicitWidth: 1
                        implicitHeight: 1
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("x: ")
                        }
                        TextField {
                            id: textInputSourceX
                            Layout.fillWidth: true
                            readOnly: !checkboxInputName.checked
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator {}
                            selectByMouse: true
                            text: liveview.position.x
                        }

                        Label {
                            text: qsTr("y: ")
                        }
                        TextField {
                            id: textInputSourceY
                            Layout.fillWidth: true
                            readOnly: !checkboxInputName.checked
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator {}
                            selectByMouse: true
                            text: liveview.position.y
                        }

                        Label {
                            text: qsTr("z: ")
                        }
                        TextField {
                            id: textInputSourceZ
                            Layout.fillWidth: true
                            readOnly: !checkboxInputName.checked
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator {}
                            selectByMouse: true
                            text: liveview.position.z
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
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

                source: display.source
                audioOut: display.audioOut

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

                    Item {
                        Layout.fillWidth: true
                    }

                    Slider {
                        id: sliderVolume

                        Layout.preferredHeight: buttonViewSettings.implicitHeight / 2
                        Layout.preferredWidth: buttonViewSettings.implicitHeight * 2
                        Layout.alignment: Qt.AlignCenter

                        value: 0.5
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
        }
    }
}
