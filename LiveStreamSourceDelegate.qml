import QtQuick 2.15
import QtQuick.Layouts 1.15

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

        RowLayout {
            anchors.fill: parent
            anchors.margins: 5

            Rectangle {
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.height
                radius: width / 2
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
    }
}
