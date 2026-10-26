// SPDX-License-Identifier: GPL-3.0-or-later
import org.deepin.dcc 1.0

// Entry in the Display page. dccData is not available in this file, so the entry follows the
// daemon's SupportedModes (empty on single-GPU machines), as upstream Touchscreen.qml follows
// Display1. In integrated mode the discrete GPU is gone but hybrid/integrated are still offered.
DccObject {
    id: root
    name: "gpu"
    parentName: "display"
    displayName: qsTr("Graphics")
    weight: 120
    visible: false

    DccDBusInterface {
        property var supportedModes
        service: "org.agino.Gpu1"
        path: "/org/agino/Gpu1"
        inter: "org.agino.Gpu1"
        connection: DccDBusInterface.SystemBus
        onSupportedModesChanged: root.visible = !!supportedModes && supportedModes.length > 0
    }
}
