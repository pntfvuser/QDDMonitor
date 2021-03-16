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
    signal mutePressed()
    signal soloPressed()
    signal recordPressed()

    id: windowMain

    width: 640
    height: 480
    visible: true
    title: qsTr("QDDMonitor")

    NumberAnimation on playbackTimer {
        running: true
        loops: Animation.Infinite
        from: 0
        to: 10000
        duration: 10000
    }

    Item {
        anchors.fill: parent

        focus: true

        ColumnLayout {
            id: columnlayoutSource

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 150

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 5
                Layout.rightMargin: 5

                columns: 2

                Label {
                    text: qsTr("Name: ")
                }
                TextField {
                    id: textInputRoomName
                    Layout.fillWidth: true

                    selectByMouse: true
                }

                Label {
                    text: qsTr("Room No.: ")
                }
                TextField {
                    id: textInputRoomId
                    Layout.fillWidth: true

                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator {}
                    selectByMouse: true
                }
            }

            Button {
                text: qsTr("Add room")

                Layout.fillWidth: true

                onClicked: {
                    sourceModelMain.addBilibiliSource(textInputRoomName.text, textInputRoomId.text);
                }
            }

            ListView {
                id: listViewSource

                Layout.fillHeight: true
                Layout.fillWidth: true

                clip: true

                model: LiveStreamSourceModel {
                    id: sourceModelMain
                }

                delegate: LiveStreamSourceDelegate {
                    anchors.left: parent.left
                    anchors.right: parent.right
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 5
                Layout.rightMargin: 5

                columns: 2

                Label {
                    text: qsTr("Rows: ")
                }
                TextField {
                    id: textInputLayoutRows
                    Layout.fillWidth: true

                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator {
                        bottom: 1
                        top: 12
                    }
                    selectByMouse: true

                    text: "3"
                }

                Label {
                    text: qsTr("Columns: ")
                }
                TextField {
                    id: textInputLayoutColumns
                    Layout.fillWidth: true

                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator {
                        bottom: 1
                        top: 12
                    }
                    selectByMouse: true

                    text: "3"
                }
            }

            Button {
                text: isLayoutEditMode ? qsTr("Save layout") : qsTr("Edit layout")

                Layout.fillWidth: true

                onClicked: {
                    if (isLayoutEditMode) {
                        viewModelMain.resetLayout(viewLayoutModelMain);
                        isLayoutEditMode = false;
                    } else {
                        if (textInputLayoutRows.acceptableInput && textInputLayoutColumns.acceptableInput) {
                            viewLayoutModelMain.resetLayout(textInputLayoutRows.text, textInputLayoutColumns.text);
                            isLayoutEditMode = true;
                        }
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

        Keys.onPressed: {
            if (event.key === Qt.Key_M) {
                event.accepted = true;
                mutePressed();
            } else if (event.key === Qt.Key_S) {
                event.accepted = true;
                soloPressed();
            } else if (event.key === Qt.Key_R) {
                event.accepted = true;
                recordPressed();
            }
        }
    }

    Component.onCompleted: {
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
