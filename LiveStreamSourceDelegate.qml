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

        Text {
            id: textSourceName

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 0

            leftPadding: 5
            rightPadding: 5
            font.pixelSize: 16

            text: display.name
        }

        MouseArea {
            id: mouseAreaSource

            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true

            onPressed: {
                listViewSource.currentIndex = index
                sourcePressed(display.id)
            }

            onReleased: {
                sourceReleased(mapToItem(null, mouse.x, mouse.y))
            }
        }
    }
}
