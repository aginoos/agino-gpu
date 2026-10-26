// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu-client/gpustate.h"

#include <QWidget>

namespace Dtk::Widget {
class DFloatingButton;
class DLabel;
}

namespace AginoGpu {

// The two-cell quick-panel tile: round mode icon, title, and a one-line status. The whole tile
// opens the sub-page; nothing toggles on click.
class Tile : public QWidget
{
    Q_OBJECT
public:
    explicit Tile(GpuState *state, QWidget *parent = nullptr);

    void refresh();

Q_SIGNALS:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    GpuState *m_state;
    Dtk::Widget::DFloatingButton *m_icon;
    Dtk::Widget::DLabel *m_title;
    Dtk::Widget::DLabel *m_subtitle;
    QPoint m_pressPoint;
};

} // namespace AginoGpu
