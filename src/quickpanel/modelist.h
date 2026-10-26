// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/types.h"

#include <QList>
#include <QWidget>

class QRadioButton;

namespace Dtk::Widget {
class DLabel;
}

namespace AginoGpu {

// One radio row per offered mode: the mode name, and on unselected rows what switching costs.
// Rows do not toggle themselves; the owner calls setModes() with the configured mode.
class ModeList : public QWidget
{
    Q_OBJECT
public:
    explicit ModeList(QWidget *parent = nullptr);

    void setModes(const QList<Mode> &offered, Mode configured, Mode active);

Q_SIGNALS:
    void modeClicked(AginoGpu::Mode mode);

private:
    void clear();
};

} // namespace AginoGpu
