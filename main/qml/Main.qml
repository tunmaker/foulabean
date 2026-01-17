import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: "DigiTwin"

    Rectangle {
        width: 400
        height: 300
        color: "lightblue"

        Text {
            text: "Hello, QML World!"
            //anchors: anchors.centerInParent
        }

        Button {
            text: "Click me"
            width: 200
            height: 50
            //anchors: anchors.centerInParent
        }
    }
}
