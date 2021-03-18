import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    property int selectedValueIndex: -1
    property string selectedValue: ""
    property var values: []

    id: itemDropUpMenu

    Rectangle {
        id: rectangleSelectedValue

        anchors.fill: parent

        color: "lightgray"

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: imageSelectedValue.left
            anchors.leftMargin: 4

            text: selectedValue
        }

        Image {
            id: imageSelectedValue

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: height

            source: "images/expand_more.svg"
        }
    }

    MouseArea {
        id: mouseareaDropUpMenu

        anchors.fill: parent

        onClicked: {
            if (!popupDropUpMenu.opened) {
                popupDropUpMenu.open();
            } else {
                popupDropUpMenu.close();
            }
        }
    }

    Popup {
        id: popupDropUpMenu

        x: 0
        y: -height
        width: parent.width
        height: listviewValues.count * parent.height

        padding: 0

        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        ListView {
            id: listviewValues

            anchors.fill: parent

            delegate: Rectangle {
                width: parent.width
                height: itemDropUpMenu.height
                color: "lightgray"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4

                    text: modelData
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        selectedValueIndex = index;
                        popupDropUpMenu.close();
                    }
                }
            }
        }
    }

    onValuesChanged: {
        var valueArray = [];
        for (var i = 0; i < values.length; ++i) {
            valueArray.push(values[i]);
        }
        listviewValues.model = valueArray;
    }
}
