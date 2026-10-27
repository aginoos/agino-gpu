// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu-client/gpucontrol.h"
#include "agino-gpu-client/gpustate.h"
#include "bannerwidget.h"
#include "modeswitcher.h"

#include <QWidget>

class QTimer;
class QVBoxLayout;

namespace Dtk::Widget {
class DLabel;
}

namespace AginoGpu {

class ModeList;

// The sub-page that opens from the tile (itemPopupApplet).
class Panel : public QWidget
{
    Q_OBJECT
public:
    explicit Panel(GpuState *state, QWidget *parent = nullptr);

    void refresh();

Q_SIGNALS:
    void requestHide();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    BannerSpec currentBanner();
    BannerSpec noticeBanner() const;
    BannerSpec pendingBanner();
    void refreshUsers();
    void showUsers(const QList<GpuUser> &users);
    void openSettings();
    void closeThen(void (*sessionRequest)());

    GpuState *m_state;
    GpuControl *m_control;
    ModeSwitcher *m_switcher;
    Dtk::Widget::DLabel *m_dgpuStatus;
    BannerWidget *m_banner;
    ModeList *m_modes;
    QWidget *m_usersGroup;
    QVBoxLayout *m_usersLayout;
    QTimer *m_usersTimer;
    bool m_usersPending = false;
    bool m_polling = false;
};

} // namespace AginoGpu
