import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QmlMiniEditor

ApplicationWindow {
    id: window
    width: 1280
    height: 760
    visible: true
    title: "QML Mini Editor — Lesson 1"
    color: "#17191f"

    EditorViewModel {
        id: editor
    }

    component SectionTitle: Text {
        color: "#f0f2f5"
        font.pixelSize: 15
        font.bold: true
    }

    Rectangle {
        id: topBar
        height: 48
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#101216"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 18

            Text { text: "▶"; color: "#42a5f5"; font.pixelSize: 20 }
            Text { text: "File"; color: "#e4e7eb" }
            Text { text: "Edit"; color: "#e4e7eb" }
            Text { text: "Playback"; color: "#e4e7eb" }
            Item { Layout.fillWidth: true }
            Text { text: "Untitled QML Project"; color: "#e4e7eb" }
        }
    }

    RowLayout {
        anchors.top: topBar.bottom
        anchors.bottom: timeline.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 290
            Layout.fillHeight: true
            color: "#22252d"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                SectionTitle { text: "Media Library" }
                Text {
                    text: "QAbstractListModel roles → QML delegate"
                    color: "#95a0b2"
                    font.pixelSize: 11
                }

                ListView {
                    id: library
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: editor.mediaLibrary

                    delegate: Rectangle {
                        required property int index
                        required property string title
                        required property string kind
                        required property string duration
                        required property color accentColor

                        width: library.width
                        height: 76
                        radius: 3
                        color: index === editor.selectedAssetIndex ? "#294f7e" : "#2b2f38"
                        border.width: index === editor.selectedAssetIndex ? 2 : 0
                        border.color: "#42a5f5"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 76
                                Layout.fillHeight: true
                                color: accentColor
                                Text {
                                    anchors.centerIn: parent
                                    text: kind
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: title; color: "white"; elide: Text.ElideRight; Layout.fillWidth: true }
                                Text { text: duration; color: "#b9c5d4"; font.pixelSize: 12 }
                            }
                        }
                        TapHandler { onTapped: editor.selectAsset(index) }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0a0b0e"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                SectionTitle { text: "Preview" }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#101216"
                    border.width: 1
                    border.color: "#363b46"

                    Column {
                        anchors.centerIn: parent
                        spacing: 8
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Selected source"; color: "#95a0b2" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: editor.selectedAssetTitle; color: "white"; font.pixelSize: 22 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Fake preview — no decoder in Lesson 1"; color: "#42a5f5" }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 270
            Layout.fillHeight: true
            color: "#22252d"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                SectionTitle { text: "Properties" }
                Text { text: "Selected asset"; color: "#95a0b2" }
                Text { text: editor.selectedAssetTitle; color: "white"; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#3a404c" }
                Text { text: "Lesson: Q_PROPERTY binding"; color: "#42a5f5"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Item { Layout.fillHeight: true }
            }
        }
    }

    Rectangle {
        id: timeline
        height: 190
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "#1d2027"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            RowLayout {
                SectionTitle { text: "Timeline" }
                Item { Layout.fillWidth: true }
                Text { text: "Lesson 1: QML layout only"; color: "#95a0b2" }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#14161b"
                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 2
                    Repeater {
                        model: 3
                        delegate: Rectangle {
                            width: parent.width / 3 - 2
                            height: parent.height
                            color: index === editor.selectedAssetIndex ? "#3d77b6" : "#315d46"
                            border.width: 1
                            border.color: "#93c9ff"
                            Text {
                                anchors.centerIn: parent
                                text: index === 0 ? "V1  Mountainbike.mp4" : index === 1 ? "V1  Forest.jpg" : "A1  Mahoroba.mp3"
                                color: "white"
                            }
                        }
                    }
                }
            }
        }
    }
}
