// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import org.deepin.dtk 1.0 as D

// The single banner above the page: text, optional details, action buttons.
Rectangle {
    id: control

    property var banner: ({})
    property bool detailsShown: false

    readonly property color tint: banner.kind === "error" ? "#e5484d"
                                : banner.kind === "warning" ? "#f5a623" : "#3e8ef7"

    radius: 8
    color: Qt.rgba(tint.r, tint.g, tint.b, 0.16)
    border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.5)
    implicitHeight: layout.implicitHeight + 16
    Layout.fillWidth: true

    onBannerChanged: detailsShown = false

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 8
        anchors.leftMargin: 14
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: control.banner.text || ""
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: control.banner.actions || []
                delegate: D.Button {
                    required property var modelData
                    text: modelData.id === "dismiss" ? "×" : modelData.label
                    flat: modelData.id === "dismiss" || modelData.id === "details"
                    highlighted: modelData.primary === true
                    enabled: !dccData.busy
                    onClicked: {
                        if (modelData.id === "details")
                            control.detailsShown = !control.detailsShown
                        else
                            dccData.bannerAction(modelData.id)
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: control.detailsShown && text !== ""
            text: control.banner.detail || ""
            wrapMode: Text.WordWrap
            opacity: 0.7
        }
    }
}
