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

    LiveStreamView {
        id: liveview
        anchors.fill: parent

        source: display.source
        audioOut: display.audioOut

        t: playbackTimer
    }
}
