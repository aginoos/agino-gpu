// SPDX-License-Identifier: GPL-3.0-or-later
import org.deepin.dcc 1.0

// Entry in the Display page. This file cannot use dccData: GpuMain.qml shows the entry once
// the model reports more than one GPU.
DccObject {
    name: "gpu"
    parentName: "display"
    displayName: qsTr("Graphics")
    weight: 120
    visible: false
}
