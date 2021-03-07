import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.anon.QDDMonitor 1.0

Item {
    id: itemViewDelegate

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

        volume: volumeSlider.position

        t: playbackTimer
    }

    Rectangle {
        id: rectangleToolbar

        height: 50
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
                id: volumeSlider

                Layout.preferredHeight: 25
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignCenter

                value: 0.5
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
