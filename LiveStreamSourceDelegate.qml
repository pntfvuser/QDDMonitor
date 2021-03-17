import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: itemSourceDelegate
    height: textSourceName.implicitHeight + 20

    Item {
        anchors.fill: parent
        anchors.margins: 5

        Rectangle {
            id: rectSourceHoverHighlight
            anchors.fill: parent
            visible: itemSourceDelegate.ListView.isCurrentItem || mouseAreaSource.containsMouse
            color: itemSourceDelegate.ListView.isCurrentItem ? "lightgray" : "whitesmoke"
        }

        MouseArea {
            id: mouseAreaSource

            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true

            onPressed: {
                listViewSource.currentIndex = index
                sourcePressed(display.id, -1)
            }

            onReleased: {
                sourceReleased(mapToItem(null, mouse.x, mouse.y))
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 5

            Rectangle {
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.height
                radius: width * 0.5
                color: display.online ? "green" : "red"
            }

            Text {
                id: textSourceName

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter

                font.pixelSize: 16

                text: display.name
            }

            Button {
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.height

                background: Rectangle {
                    color: parent.pressed ? "gray" : "lightgray"
                    Image {
                        anchors.fill: parent
                        source: "images/remove_circle.svg"
                    }
                }

                onClicked: {
                    sourceModelMain.removeSourceByIndex(index)
                }
            }
        }
    }
}
