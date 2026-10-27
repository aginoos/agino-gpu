// SPDX-License-Identifier: GPL-3.0-or-later
#include "panel.h"

#include "agino-gpu-client/powerwatcher.h"
#include "agino-gpu-client/texts.h"
#include "constants.h"
#include "controlcenter.h"
#include "itemrow.h"
#include "modelist.h"
#include "qptexts.h"

#include <DFontSizeManager>
#include <DLabel>

#include <QHBoxLayout>
#include <QIcon>
#include <QTimer>
#include <QVBoxLayout>

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

namespace AginoGpu {

namespace {
constexpr int kUsersIntervalMs = 2000;
// dde-tray-loader's sub-pages: 10 px from the popup edge and between sections.
constexpr int kSidePadding = 10;
constexpr int kSectionSpacing = 10;
constexpr int kUsersSpacing = 4;
// The X server keeps the discrete GPU open in hybrid mode (its GPU screen) without waking it;
// it is not an app the user started, so the list leaves it out.
const QStringList kDisplayServers = { QStringLiteral("Xorg"), QStringLiteral("Xwayland") };
// Restart and log out close the panel first; the session request follows once it has gone, or
// the panel stays on the darkening screen until the session ends.
constexpr int kSessionRequestDelayMs = 300;
}

Panel::Panel(GpuState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_control(new GpuControl(this))
    , m_switcher(new ModeSwitcher(state, m_control, this))
    , m_banner(new BannerWidget(this))
    , m_modes(new ModeList(this))
    , m_usersGroup(new QWidget(this))
    , m_usersLayout(new QVBoxLayout)
    , m_usersTimer(new QTimer(this))
{
    setFixedWidth(Dock::DOCK_POPUP_WIDGET_WIDTH);

    // The quick panel draws the "‹ Graphics" title bar of the sub-page itself (dde-shell
    // SubPluginPage, from pluginDisplayName); the first row here is the discrete GPU's status,
    // as a section caption like Bluetooth's "My Devices".
    auto *dgpuLabel = createSectionLabel(QpTexts::discreteGpu(), this);
    m_dgpuStatus = createSectionLabel(QString(), this);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(kSidePadding, 0, kSidePadding, 0);
    header->addWidget(dgpuLabel);
    header->addStretch(1);
    header->addWidget(m_dgpuStatus);

    auto *usersTitle = createSectionLabel(QpTexts::dgpuAppsTitle(), m_usersGroup);
    m_usersLayout->setContentsMargins(kSidePadding, 0, kSidePadding, 0);
    m_usersLayout->setSpacing(kUsersSpacing);
    m_usersLayout->addWidget(usersTitle);
    m_usersGroup->setLayout(m_usersLayout);
    m_usersGroup->setVisible(false);

    auto *settings = new ItemRow(this);
    settings->addIcon(QIcon::fromTheme(QStringLiteral("agino-gpu-open-arrow")));
    settings->addLabel(Texts::settings(), ItemRow::Text::Primary);

    auto *content = new QVBoxLayout;
    content->setContentsMargins(kSidePadding, 0, kSidePadding, 0);
    content->setSpacing(kSectionSpacing);
    content->addWidget(m_banner);
    content->addWidget(m_modes);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, kSidePadding, 0, kSidePadding);
    layout->setSpacing(kSectionSpacing);
    layout->addLayout(header);
    layout->addLayout(content);
    layout->addWidget(m_usersGroup);
    layout->addStretch(1);
    auto *footer = new QVBoxLayout;
    footer->setContentsMargins(kSidePadding, 0, kSidePadding, 0);
    footer->addWidget(settings);
    layout->addLayout(footer);

    m_usersTimer->setInterval(kUsersIntervalMs);
    connect(settings, &ItemRow::clicked, this, &Panel::openSettings);
    connect(m_modes, &ModeList::modeClicked, m_switcher, &ModeSwitcher::select);
    connect(m_switcher, &ModeSwitcher::changed, this, &Panel::refresh);
    connect(m_usersTimer, &QTimer::timeout, this, &Panel::refreshUsers);
    connect(m_state, &GpuState::changed, this, &Panel::refresh);
    connect(m_state, &GpuState::dgpuStatusChanged, this, &Panel::refresh);

    refresh();
}

void Panel::refresh()
{
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

BannerSpec Panel::pendingBanner()
{
    const PendingAction action = m_state->pendingAction();
    if (action == PendingAction::None)
        return {};
    BannerSpec spec { Texts::pendingBanner(m_state->configuredMode(), action), {} };
    spec.actions << BannerAction { Texts::undo(), [this] { m_switcher->undo(); } };
    if (action == PendingAction::Reboot)
        spec.actions << BannerAction { Texts::restartNow(), [this] { closeThen(&Session::requestReboot); } };
    else
        spec.actions << BannerAction { Texts::logoutNow(), [this] { closeThen(&Session::requestLogout); } };
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
        if (!user.name.isEmpty() && !names.contains(user.name) && !kDisplayServers.contains(user.name))
            names << user.name;
    }
    for (const QString &name : names) {
        auto *label = new DLabel(name, m_usersGroup);
        label->setElideMode(Qt::ElideRight);
        label->setForegroundRole(QPalette::BrightText);
        DFontSizeManager::instance()->bind(label, DFontSizeManager::T8);
        m_usersLayout->addWidget(label);
    }
    m_usersGroup->setVisible(!names.isEmpty());
}

void Panel::closeThen(void (*sessionRequest)())
{
    Q_EMIT requestHide();
    QTimer::singleShot(kSessionRequestDelayMs, this, sessionRequest);
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
