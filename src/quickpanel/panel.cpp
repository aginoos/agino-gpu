// SPDX-License-Identifier: GPL-3.0-or-later
#include "panel.h"

#include "agino-gpu-client/powerwatcher.h"
#include "agino-gpu-client/texts.h"
#include "constants.h"
#include "controlcenter.h"
#include "modelist.h"
#include "qptexts.h"

#include <DFontSizeManager>
#include <DLabel>

#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

namespace AginoGpu {

namespace {
constexpr int kUsersIntervalMs = 2000;
}

Panel::Panel(GpuState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_control(new GpuControl(this))
    , m_switcher(new ModeSwitcher(state, m_control, this))
    , m_back(new QPushButton(this))
    , m_dgpuStatus(new DLabel(this))
    , m_banner(new BannerWidget(this))
    , m_modes(new ModeList(this))
    , m_usersGroup(new QWidget(this))
    , m_usersLayout(new QVBoxLayout)
    , m_usersTimer(new QTimer(this))
{
    setFixedWidth(Dock::DOCK_POPUP_WIDGET_WIDTH);

    m_back->setFlat(true);
    m_back->setFocusPolicy(Qt::NoFocus);
    m_back->setCursor(Qt::PointingHandCursor);
    m_back->setStyleSheet(QStringLiteral("text-align:left;"));
    DFontSizeManager::instance()->bind(m_back, DFontSizeManager::T5);
    DFontSizeManager::instance()->bind(m_dgpuStatus, DFontSizeManager::T8);
    m_dgpuStatus->setForegroundRole(QPalette::PlaceholderText);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(10, 0, 10, 0);
    header->addWidget(m_back);
    header->addStretch(1);
    header->addWidget(m_dgpuStatus);

    auto *usersTitle = new DLabel(QpTexts::dgpuAppsTitle(), m_usersGroup);
    DFontSizeManager::instance()->bind(usersTitle, DFontSizeManager::T6);
    usersTitle->setForegroundRole(QPalette::PlaceholderText);
    m_usersLayout->setContentsMargins(0, 0, 0, 0);
    m_usersLayout->setSpacing(4);
    m_usersLayout->addWidget(usersTitle);
    m_usersGroup->setLayout(m_usersLayout);
    m_usersGroup->setContentsMargins(20, 0, 20, 0);
    m_usersGroup->setVisible(false);

    auto *settings = new QPushButton(Texts::settings(), this);
    settings->setFlat(true);
    settings->setFocusPolicy(Qt::NoFocus);
    settings->setCursor(Qt::PointingHandCursor);

    auto *bannerHolder = new QHBoxLayout;
    bannerHolder->setContentsMargins(10, 0, 10, 0);
    bannerHolder->addWidget(m_banner);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 10);
    layout->setSpacing(8);
    layout->addLayout(header);
    layout->addLayout(bannerHolder);
    layout->addWidget(m_modes);
    layout->addWidget(m_usersGroup);
    layout->addStretch(1);
    layout->addWidget(settings);

    m_usersTimer->setInterval(kUsersIntervalMs);
    connect(m_back, &QPushButton::clicked, this, &Panel::requestHide);
    connect(settings, &QPushButton::clicked, this, &Panel::openSettings);
    connect(m_modes, &ModeList::modeClicked, m_switcher, &ModeSwitcher::select);
    connect(m_switcher, &ModeSwitcher::changed, this, &Panel::refresh);
    connect(m_usersTimer, &QTimer::timeout, this, &Panel::refreshUsers);
    connect(m_state, &GpuState::changed, this, &Panel::refresh);
    connect(m_state, &GpuState::dgpuStatusChanged, this, &Panel::refresh);

    refresh();
}

void Panel::refresh()
{
    m_back->setText(QStringLiteral("‹ ") + Texts::title());
    m_dgpuStatus->setText(Texts::dgpuStatus(m_state->dgpuStatus()));
    m_banner->setSpec(currentBanner());
    m_modes->setModes(m_state->offeredModes(), m_state->configuredMode(), m_state->activeMode());
}

BannerSpec Panel::currentBanner()
{
    if (!m_switcher->notice().isEmpty())
        return noticeBanner();
    if (!m_state->lastFailure().isEmpty()) {
        return { Texts::failureBanner(m_state->failedMode()),
                 { { Texts::details(), [this] { openSettings(); } } } };
    }
    return pendingBanner();
}

BannerSpec Panel::noticeBanner() const
{
    const ModeSwitcher::Notice &notice = m_switcher->notice();
    if (!notice.needsConfirm)
        return { notice.text, {} };
    return { notice.text,
             { { QpTexts::confirmSwitch(), [this] { m_switcher->confirm(); } },
               { QpTexts::cancel(), [this] { m_switcher->clearNotice(); } } } };
}

BannerSpec Panel::pendingBanner() const
{
    const PendingAction action = m_state->pendingAction();
    if (action == PendingAction::None)
        return {};
    BannerSpec spec { Texts::pendingBanner(m_state->configuredMode(), action), {} };
    spec.actions << BannerAction { Texts::undo(), [this] { m_switcher->undo(); } };
    if (action == PendingAction::Reboot)
        spec.actions << BannerAction { Texts::restartNow(), [] { Session::requestReboot(); } };
    else
        spec.actions << BannerAction { Texts::logoutNow(), [] { Session::requestLogout(); } };
    return spec;
}

void Panel::refreshUsers()
{
    if (m_usersPending)
        return;
    m_usersPending = true;
    m_control->dgpuUsers([this](const QList<GpuUser> &users) {
        m_usersPending = false;
        showUsers(users);
    });
}

void Panel::showUsers(const QList<GpuUser> &users)
{
    while (m_usersLayout->count() > 1) {
        QLayoutItem *item = m_usersLayout->takeAt(1);
        delete item->widget();
        delete item;
    }
    QStringList names;
    for (const GpuUser &user : users) {
        if (!user.name.isEmpty() && !names.contains(user.name))
            names << user.name;
    }
    for (const QString &name : names)
        m_usersLayout->addWidget(new DLabel(name, m_usersGroup));
    m_usersGroup->setVisible(!names.isEmpty());
}

void Panel::openSettings()
{
    openControlCenterGpuPage();
    Q_EMIT requestHide();
}

void Panel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_polling)
        m_state->setPolling(true);
    m_polling = true;
    refresh();
    refreshUsers();
    m_usersTimer->start();
}

void Panel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_usersTimer->stop();
    if (m_polling)
        m_state->setPolling(false);
    m_polling = false;
    m_switcher->clearNotice();
}

} // namespace AginoGpu
