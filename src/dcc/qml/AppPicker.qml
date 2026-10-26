// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dtk 1.0 as D

// The installed applications as a ticked list. Rows that already have a rule are ticked and
// cannot be changed.
Rectangle {
    id: control

    property alias model: repeater.model

    color: Qt.rgba(0.5, 0.5, 0.5, 0.1)
    radius: 8
    implicitHeight: column.implicitHeight

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        Repeater {
            id: repeater
            delegate: ItemDelegate {
                id: row

                Layout.fillWidth: true
                enabled: !model.alreadyAdded
                onClicked: control.model.toggle(index)

                contentItem: RowLayout {
                    spacing: 10
                    D.IconLabel {
                        icon.name: model.icon
                        icon.width: 24
                        icon.height: 24
                    }
                    Label {
                        Layout.fillWidth: true
                        text: model.name
                        elide: Text.ElideRight
                    }
                    CheckBox {
                        id: tick
                        checked: model.checked
                        enabled: !model.alreadyAdded
                        onToggled: control.model.toggle(index)
                        Binding {
                            target: tick
                            property: "checked"
                            value: model.checked
                        }
                    }
                }
            }
        }
    }
}
