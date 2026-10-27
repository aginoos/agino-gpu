// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/types.h"

#include <QList>
#include <QWidget>

namespace AginoGpu {

// One row per offered mode: the mode name, a check mark on the saved mode and, on the others,
// what switching costs.
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

    bool m_built = false;
    QList<Mode> m_offered;
    Mode m_configured = Mode::Hybrid;
    Mode m_active = Mode::Hybrid;
};

} // namespace AginoGpu
