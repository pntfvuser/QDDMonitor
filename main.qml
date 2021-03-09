import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.anon.QDDMonitor 1.0

Window {
    property int selectedSource: -1
    property int selectedSourceView: -1
    property bool isLayoutEditMode: false

    property real playbackTimer: 0

    signal sourcePressed(int sourceId, int viewIndex)
    signal sourceReleased(point releasePoint)

    width: 640
    height: 480
    visible: true
    title: "UI Test"

    NumberAnimation on playbackTimer {
        running: true
        loops: Animation.Infinite
        from: 0
        to: 10000
        duration: 10000
    }

    ColumnLayout {
        id: columnlayoutSource

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 100

        ListView {
            id: listViewSource

            Layout.fillHeight: true
            Layout.fillWidth: true

            model: LiveStreamSourceModel {
                id: sourceModelMain
            }

            delegate: LiveStreamSourceDelegate {
                anchors.left: parent.left
                anchors.right: parent.right
            }
        }

        Button {
            text: "switch view mode"

            Layout.fillWidth: true

            onPressed: {
                if (isLayoutEditMode) {
                    viewModelMain.resetLayout(viewLayoutModelMain);
                    isLayoutEditMode = false;
                } else {
                    viewLayoutModelMain.resetLayout(5, 5);
                    isLayoutEditMode = true;
                }
            }
        }
    }

    Item {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: columnlayoutSource.right
        anchors.right: parent.right

        Item {
            anchors.fill: parent

            enabled: !isLayoutEditMode
            visible: !isLayoutEditMode

            FixedGridLayout {
                id: gridView

                anchors.fill: parent

                enabled: !isLayoutEditMode
                visible: !isLayoutEditMode

                rows: viewModelMain.rows
                columns: viewModelMain.columns

                Repeater {
                    id: repeaterViews

                    model: LiveStreamViewModel {
                        id: viewModelMain
                        sourceModel: sourceModelMain
                    }

                    delegate: LiveStreamViewDelegate {
                        FixedGridLayout.row: display.row
                        FixedGridLayout.column: display.column
                        FixedGridLayout.rowSpan: display.rowSpan
                        FixedGridLayout.columnSpan: display.columnSpan
                    }
                }
            }
        }

        Item {
            anchors.fill: parent

            enabled: isLayoutEditMode
            visible: isLayoutEditMode

            MouseArea {
                property real columnWidth: width / viewLayoutModelMain.columns
                property real rowHeight: height / viewLayoutModelMain.rows
                property int beginRow: -1
                property int beginColumn: -1
                property int endRow: -1
                property int endColumn: -1
                property int row: Math.min(beginRow, endRow)
                property int column: Math.min(beginColumn, endColumn)
                property int rowSpan: Math.abs(beginRow - endRow) + 1
                property int columnSpan: Math.abs(beginColumn - endColumn) + 1
                property bool intersecting: viewLayoutModelMain.itemWillIntersect(row, column, rowSpan, columnSpan)

                id: mouseareaViewLayoutEditing

                anchors.fill: parent
                preventStealing: true

                onPressed: {
                    beginColumn = endColumn = mouse.x / columnWidth;
                    beginRow = endRow = mouse.y / rowHeight;
                }

                onPositionChanged: {
                    endColumn = mouse.x / columnWidth;
                    endRow = mouse.y / rowHeight;
                }

                onReleased: {
                    if (!containsMouse) {
                        return;
                    }
                    endColumn = mouse.x / columnWidth;
                    endRow = mouse.y / rowHeight;
                    if (!intersecting) {
                        viewLayoutModelMain.addLayoutItem(row, column, rowSpan, columnSpan);
                    }
                    beginColumn = endColumn = beginRow = endRow = -1;
                }
            }

            FixedGridLayout {
                id: viewgridViewLayout

                anchors.fill: parent

                rows: viewLayoutModelMain.rows
                columns: viewLayoutModelMain.columns

                Rectangle {
                    enabled: mouseareaViewLayoutEditing.pressed
                    visible: mouseareaViewLayoutEditing.pressed

                    FixedGridLayout.row: mouseareaViewLayoutEditing.row
                    FixedGridLayout.column: mouseareaViewLayoutEditing.column
                    FixedGridLayout.rowSpan: mouseareaViewLayoutEditing.rowSpan
                    FixedGridLayout.columnSpan: mouseareaViewLayoutEditing.columnSpan

                    color: mouseareaViewLayoutEditing.intersecting ? "red" : "green"
                }

                Repeater {
                    id: repeaterViewLayouts

                    model: LiveStreamViewLayoutModel {
                        id: viewLayoutModelMain
                    }

                    delegate: Item {
                        FixedGridLayout.row: display.row
                        FixedGridLayout.column: display.column
                        FixedGridLayout.rowSpan: display.rowSpan
                        FixedGridLayout.columnSpan: display.columnSpan

                        Rectangle {
                            anchors.fill: parent

                            color: "transparent"
                            border.color: "black"
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                font.pixelSize: 30
                                text: index
                            }
                        }

                        MouseArea {
                            anchors.fill: parent

                            preventStealing: true

                            onClicked: {
                                viewLayoutModelMain.deleteLayoutItem(index);
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        sourceModelMain.addBilibiliSource("room1", 14327465);
        sourceModelMain.addBilibiliSource("room2", 8725120);
        sourceModelMain.addBilibiliSource("room3", 6655);
        sourceModelMain.addBilibiliSource("room4", 766);
    }

    onSourcePressed: {
        selectedSource = sourceId;
        selectedSourceView = viewIndex;
    }

    onSourceReleased: {
        for (var i = repeaterViews.count - 1; i >= 0; --i) {
            var item = repeaterViews.itemAt(i);
            console.assert(item !== null);
            if (item.contains(item.mapFromItem(null, releasePoint))) {
                if (i !== selectedSourceView) {
                    if (selectedSourceView == -1) {
                        viewModelMain.setSource(i, selectedSource);
                    } else {
                        viewModelMain.swapSource(i, selectedSourceView);
                    }
                }
                break;
            }
        }

        selectedSource = -1;
        selectedSourceView = -1;
    }
}
