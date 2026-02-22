import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import digitwin

ApplicationWindow {
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: "DigiTwin — Bridge Test Harness"

    SimulationController {
        id: sim
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16

        ColumnLayout {
            width: parent.width
            spacing: 16

            // ============================================================
            // Section 1: Connection
            // ============================================================
            GroupBox {
                title: "Connection"
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: {
                            if (sim.connecting) return "Status: Connecting..."
                            if (sim.connected) return "Status: Connected to " + sim.machineName + " (" + sim.machineId + ")"
                            return "Status: Disconnected"
                        }
                        font.bold: true
                        color: sim.connected ? "#2e7d32" : sim.connecting ? "#f57f17" : "#c62828"
                    }

                    Label {
                        text: "Error: " + sim.connectionError
                        color: "#c62828"
                        visible: sim.connectionError !== ""
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        spacing: 8

                        Label {
                            text: "Test Script:"
                        }

                        ComboBox {
                            id: scriptCombo
                            model: sim.rescScriptNames
                            currentIndex: -1
                            displayText: currentIndex === -1 ? "— select a script —" : currentText
                            Layout.fillWidth: true
                            enabled: !sim.connected && !sim.connecting
                            onActivated: sim.selectScript(currentIndex)
                        }
                    }

                    RowLayout {
                        spacing: 8

                        Button {
                            text: "Connect"
                            enabled: !sim.connected && !sim.connecting && sim.selectedScript !== ""
                            onClicked: sim.connectToRenode(
                                "/home/tunmaker/packages/renode_portable/renode",
                                sim.selectedScript
                            )
                        }

                        Button {
                            text: "Disconnect"
                            enabled: sim.connected || sim.connecting
                            onClicked: sim.disconnect()
                        }
                    }
                }
            }

            // ============================================================
            // Section 2: Simulation Control
            // ============================================================
            GroupBox {
                title: "Simulation Control"
                Layout.fillWidth: true
                enabled: sim.connected

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        spacing: 16

                        Label {
                            text: "Time: " + sim.simulationTimeFormatted
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Rectangle {
                            width: 12; height: 12; radius: 6
                            color: sim.running ? "#2e7d32" : "#c62828"
                        }
                        Label {
                            text: sim.running ? "Running" : "Paused"
                        }
                    }

                    RowLayout {
                        spacing: 8

                        Button {
                            text: "Run 100ms"
                            onClicked: sim.runFor(100)
                        }

                        Button {
                            text: sim.running ? "Pause" : "Resume"
                            onClicked: sim.running ? sim.pause() : sim.resume()
                        }

                        Button {
                            text: "Reset"
                            onClicked: sim.reset()
                        }

                        Button {
                            text: "Refresh Peripherals"
                            onClicked: sim.refreshPeripherals()
                        }
                    }
                }
            }

            // ============================================================
            // Section 3: GPIO Pins
            // ============================================================
            GroupBox {
                title: "GPIO Pins (" + sim.gpioModel.pinCount + " pins)"
                Layout.fillWidth: true
                enabled: sim.connected

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        spacing: 8

                        Button {
                            text: "Set Pin 0 High"
                            onClicked: sim.setGpioPin(0, 1)
                        }
                        Button {
                            text: "Set Pin 0 Low"
                            onClicked: sim.setGpioPin(0, 0)
                        }
                    }

                    GridLayout {
                        columns: 4
                        columnSpacing: 8
                        rowSpacing: 4
                        Layout.fillWidth: true

                        Repeater {
                            model: sim.gpioModel

                            delegate: RowLayout {
                                required property int pinNumber
                                required property int pinState
                                required property string stateName
                                required property string portName

                                spacing: 4

                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: pinState === 1 ? "#2e7d32" : pinState === 0 ? "#c62828" : "#9e9e9e"
                                }

                                Label {
                                    text: "[" + portName + "] Pin " + pinNumber + ": " + stateName
                                    font.family: "monospace"
                                    Layout.preferredWidth: 160
                                }
                            }
                        }
                    }
                }
            }

            // ============================================================
            // Section 4: ADC Channels
            // ============================================================
            GroupBox {
                title: "ADC Channels (" + sim.adcModel.channelCount + " channels)"
                Layout.fillWidth: true
                enabled: sim.connected

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        spacing: 8

                        Button {
                            text: "Set Ch0 = 2.5"
                            onClicked: sim.setAdcChannel(0, 2.5)
                        }
                        Button {
                            text: "Set Ch0 = 0.0"
                            onClicked: sim.setAdcChannel(0, 0.0)
                        }
                    }

                    GridLayout {
                        columns: 3
                        columnSpacing: 16
                        rowSpacing: 4
                        Layout.fillWidth: true

                        Repeater {
                            model: sim.adcModel

                            delegate: Label {
                                required property int channelNumber
                                required property double value

                                text: "Ch " + channelNumber + ": " + value.toFixed(4) + " V"
                                font.family: "monospace"
                            }
                        }
                    }
                }
            }
        }
    }
}
