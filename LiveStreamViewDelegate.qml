import QtQuick 2.15
import QtQuick.Layouts 1.3
import org.anon.QDDMonitor 1.0

Item {
    id: itemViewDelegate

    Rectangle {
        anchors.fill: parent

        color: "transparent"
        border.color: "black"
        border.width: 1
    }

    MouseArea {
        id: mouseareaView
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true

        onPressed: {
            sourcePressed(display.sourceId, index)
        }

        onReleased: {
            sourceReleased(mapToItem(null, mouse.x, mouse.y))
        }
    }

    LiveStreamView {
        id: liveview
        anchors.fill: parent

        source: display.source
        audioOut: display.audioOut
        viewIndex: index

        t: playbackTimer
    }
}
