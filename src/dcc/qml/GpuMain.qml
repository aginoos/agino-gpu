// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dtk 1.0 as D
import org.deepin.dcc 1.0

// Content of the Graphics page; dccData is the C++ GpuPage model. All items hang under "gpu".
DccObject {
    id: root

    component StatusLabel: RowLayout {
        property string kind
        property string text
        spacing: 6
        Rectangle {
            visible: kind !== ""
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: kind === "active" ? "#2ea043" : "gray"
        }
        Label {
            text: parent.text
            font: D.DTK.fontManager.t8
            opacity: kind === "active" ? 1 : 0.6
        }
    }

    DccObject {
        name: "banner"
        parentName: "gpu"
        weight: 10
        visible: dccData.banner.visible === true
        pageType: DccObject.Item
        page: GpuBanner {
            banner: dccData.banner
        }
    }

    DccObject {
        name: "gpuGroup"
        parentName: "gpu"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {
            // Poll the discrete GPU's status only while the page is on screen.
            Component.onCompleted: dccData.setPageShown(true)
            Component.onDestruction: dccData.setPageShown(false)
        }
        DccRepeater {
            model: dccData.gpus.length
            delegate: DccObject {
                readonly property var gpu: dccData.gpus[index] || ({})
                name: "gpu" + index
                parentName: "gpu/gpuGroup"
                weight: 10 + index
                displayName: gpu.name || ""
                description: gpu.line || ""
                pageType: DccObject.Editor
                page: StatusLabel {
                    kind: gpu.statusKind || ""
                    text: gpu.status || ""
                }
            }
        }
    }

    DccTitleObject {
        name: "modeTitle"
        parentName: "gpu"
        displayName: qsTr("Mode")
        weight: 30
    }
    DccObject {
        name: "modes"
        parentName: "gpu"
        weight: 40
        pageType: DccObject.Item
        page: GpuModeCards {}
    }

    DccTitleObject {
        name: "appsTitle"
        parentName: "gpu"
        displayName: qsTr("Apps using the discrete GPU")
        description: dccData.rulesEnabled ? "" : qsTr("Hybrid mode only")
        weight: 50
    }
    DccObject {
        name: "rulesGroup"
        parentName: "gpu"
        weight: 60
        enabled: dccData.rulesEnabled
        pageType: DccObject.Item
        page: DccGroupView {}

        DccRepeater {
            model: dccData.apps.length
            delegate: DccObject {
                readonly property var rule: dccData.apps[index] || ({})
                name: "app" + index
                parentName: "gpu/rulesGroup"
                weight: 100 + index
                displayName: rule.name || ""
                icon: rule.icon || ""
                enabled: dccData.rulesEnabled
                pageType: DccObject.Editor
                page: ToolButton {
                    text: "×"
                    flat: true
                    Accessible.name: qsTr("Remove")
                    onClicked: dccData.removeApp(rule.appId)
                }
            }
        }
        DccRepeater {
            model: dccData.paths.length
            delegate: DccObject {
                readonly property var rule: dccData.paths[index] || ({})
                name: "path" + index
                parentName: "gpu/rulesGroup"
                weight: 5000 + index
                displayName: rule.name || ""
                description: rule.exists ? (rule.directory || "") : qsTr("File not found") + " · " + (rule.directory || "")
                icon: rule.icon || ""
                enabled: dccData.rulesEnabled
                pageType: DccObject.Editor
                page: ToolButton {
                    text: "×"
                    flat: true
                    Accessible.name: qsTr("Remove")
                    onClicked: dccData.removePath(rule.path)
                }
            }
        }
        AddAppPage {
            name: "addApp"
            parentName: "gpu/rulesGroup"
            displayName: qsTr("Add")
            weight: 9000
            enabled: dccData.rulesEnabled
        }
        DccObject {
            name: "pauseOnBattery"
            parentName: "gpu/rulesGroup"
            displayName: qsTr("Pause on battery")
            weight: 9100
            enabled: dccData.rulesEnabled
            backgroundType: DccObject.Normal
            pageType: DccObject.Editor
            page: Switch {
                checked: dccData.pauseOnBattery
                onClicked: {
                    dccData.pauseOnBattery = checked
                    // a click replaces the binding; restore it so later changes show
                    checked = Qt.binding(function () { return dccData.pauseOnBattery })
                }
            }
        }
    }
}
