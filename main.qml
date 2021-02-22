import QtQuick 2.15
import QtQuick.Window 2.15
import org.anon.QDDMonitor 1.0

Window {
    width: 640
    height: 480
    visible: true
    title: "Video Playback Test"

    D3D11FlushHelper {}

    AudioOutput {
        id: mainAudioOut
    }

    LiveStreamView {
        anchors.fill: parent
        audioOut: mainAudioOut
        NumberAnimation on t {
            from: 0
            to: 10000
            duration: 10000
            running: true
            loops: Animation.Infinite
        }
        Component.onCompleted: {
            source = debugSource
        }
    }
}
