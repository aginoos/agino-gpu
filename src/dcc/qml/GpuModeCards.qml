// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import org.deepin.dtk 1.0 as D

// The mode cards: name and one-line summary; unselected cards also show what the switch costs.
GridLayout {
    columns: 4
    columnSpacing: 10
    rowSpacing: 10
    Layout.fillWidth: true

    Repeater {
        model: dccData.modes.length
        delegate: Control {
            id: card
            required property int index
            readonly property var modelData: dccData.modes[index] || ({})
            readonly property bool usable: modelData.enabled && !dccData.busy

            Layout.fillWidth: true
            Layout.preferredWidth: 1
            padding: 12
            opacity: modelData.enabled ? 1 : 0.4

            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.name
            Accessible.checked: modelData.selected

            background: Rectangle {
                radius: 10
                color: mouse.containsMouse && card.usable ? Qt.rgba(0.5, 0.5, 0.5, 0.2) : Qt.rgba(0.5, 0.5, 0.5, 0.1)
                border.width: 2
                border.color: card.modelData.selected ? card.palette.highlight : "transparent"
            }
            contentItem: ColumnLayout {
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: card.modelData.name
                        font: D.DTK.fontManager.t6
                        elide: Text.ElideRight
                    }
                    Label {
                        visible: text !== ""
                        text: card.modelData.cost
                        font: D.DTK.fontManager.t10
                        color: card.palette.highlight
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: card.modelData.summary
                    font: D.DTK.fontManager.t10
                    opacity: 0.6
                    wrapMode: Text.WordWrap
                }
            }
            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: card.usable
                onClicked: dccData.selectMode(card.modelData.id)
            }
        }
    }
}
