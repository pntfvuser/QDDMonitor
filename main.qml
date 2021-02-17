import QtQuick 2.15
import QtQuick.Window 2.15
import org.anon.QDDMonitor 1.0

Window {
    width: 640
    height: 480
    visible: true
    title: "Video Playback Test"

    D3D11FlushHelper {}

    LiveStreamView {
        anchors.fill: parent
        source: LiveStreamSource {
            id: source
            Component.onCompleted: {
                source.start()
            }
        }
    }
}
