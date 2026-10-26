// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Controls 2.0
import Qt.labs.platform 1.1 as P
import QtQuick.Layouts 1.15

import org.deepin.dtk 1.0 as D
import org.deepin.dcc 1.0

// "Add application" sub-page of the Graphics page: a settings page with a body (search, the
// installed apps, other programs) and a Cancel / Add footer.
DccObject {
    id: root

    readonly property string path: parentName + "/" + name
    readonly property var model: dccData.addApp

    function backToGpuPage() {
        DccApp.showPage(DccApp.object("gpu"))
    }

    page: DccSettingsView {
        Component.onCompleted: root.model.reset()
    }

    // Qt.labs.platform like DCC's own plugins (NetFileChooseEdit.qml): it goes through the DTK
    // platform dialog and can be parented to the DCC window.
    P.FileDialog {
        id: fileDialog
        title: qsTr("Choose a program")
        fileMode: P.FileDialog.OpenFile
        parentWindow: DccApp.mainWindow()
        nameFilters: [qsTr("All files (*)"), qsTr("Launchers (*.desktop)"), qsTr("AppImage (*.AppImage)")]
        onAccepted: root.model.setFile(file)
    }

    DccObject {
        name: "body"
        parentName: root.path
        weight: 10
        pageType: DccObject.Item

        DccObject {
            name: "search"
            parentName: root.path + "/body"
            weight: 10
            canSearch: false
            pageType: DccObject.Item
            page: D.SearchEdit {
                placeholder: qsTr("Search")
                text: root.model.filter
                onTextChanged: root.model.filter = text
            }
        }
        DccObject {
            name: "installed"
            parentName: root.path + "/body"
            weight: 20
            canSearch: false
            pageType: DccObject.Item
            page: AppPicker {
                model: root.model
            }
        }
        DccTitleObject {
            name: "otherTitle"
            parentName: root.path + "/body"
            displayName: qsTr("Other programs")
            description: qsTr("Works when started from the launcher")
            weight: 30
        }
        DccObject {
            name: "otherGroup"
            parentName: root.path + "/body"
            weight: 40
            canSearch: false
            pageType: DccObject.Item
            page: DccGroupView {}

            DccObject {
                name: "chooseFile"
                parentName: root.path + "/body/otherGroup"
                displayName: root.model.hasFile ? root.model.fileName : qsTr("Choose file…")
                description: root.model.hasFile ? root.model.fileDirectory : ""
                weight: 10
                backgroundType: DccObject.ClickStyle
                pageType: DccObject.Editor
                page: ToolButton {
                    visible: root.model.hasFile
                    text: "×"
                    flat: true
                    Accessible.name: qsTr("Remove")
                    onClicked: root.model.clearFile()
                }
                onActive: fileDialog.open()
            }
            DccObject {
                name: "addToLauncher"
                parentName: root.path + "/body/otherGroup"
                displayName: qsTr("Add to launcher")
                weight: 20
                visible: root.model.hasFile && !root.model.fileIsDesktopEntry
                pageType: DccObject.Editor
                page: Switch {
                    checked: root.model.createLauncher
                    onClicked: root.model.createLauncher = checked
                }
            }
        }
    }

    DccObject {
        name: "footer"
        parentName: root.path
        weight: 20
        pageType: DccObject.Item

        DccObject {
            name: "spacer"
            parentName: root.path + "/footer"
            weight: 10
            pageType: DccObject.Item
            page: Item {
                Layout.fillWidth: true
            }
        }
        DccObject {
            name: "cancel"
            parentName: root.path + "/footer"
            weight: 20
            pageType: DccObject.Item
            page: Button {
                text: qsTr("Cancel")
                onClicked: root.backToGpuPage()
            }
        }
        DccObject {
            name: "add"
            parentName: root.path + "/footer"
            weight: 30
            pageType: DccObject.Item
            page: D.RecommandButton {
                text: root.model.selectedCount > 0 ? qsTr("Add (%1)").arg(root.model.selectedCount) : qsTr("Add")
                enabled: root.model.selectedCount > 0
                onClicked: {
                    if (root.model.commit())
                        root.backToGpuPage()
                }
            }
        }
    }
}
