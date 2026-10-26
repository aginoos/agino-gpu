// SPDX-License-Identifier: GPL-3.0-or-later
// The mode-switch flow of GpuPage, identical to the quick panel: CheckMode first (an error is
// shown and stops, warnings wait for the user), then SetMode.
#include "gpupage.h"

#include "agino-gpu-client/powerwatcher.h"
#include "agino-gpu-client/rulestore.h"
#include "agino-gpu-client/texts.h"
#include "agino-gpu/mode.h"

#include <QCoreApplication>

namespace AginoGpu {

void GpuPage::selectMode(const QString &modeId)
{
    const std::optional<Mode> mode = modeFromString(modeId);
    if (!mode || m_busy || *mode == m_state->configuredMode() || !m_state->offeredModes().contains(*mode))
        return;
    clearFlow();
    // Going back to what is running needs no checks.
    if (*mode == m_state->activeMode())
        applyMode(*mode, false);
    else
        checkAndSwitch(*mode);
}

void GpuPage::checkAndSwitch(Mode mode)
{
    m_busy = true;
    refreshBanner();
    m_control->checkMode(mode, [this, mode](const QList<CheckItem> &items) {
        const QStringList outputs = dgpuOutputs();
        QStringList warnings;
        for (const CheckItem &item : items) {
            if (item.level == CheckItem::Level::Error) {
                m_busy = false;
                setFlow(FlowNotice::Kind::Error, Texts::checkText(item, outputs), mode);
                return;
            }
            if (item.level == CheckItem::Level::Warning)
                warnings.append(Texts::checkText(item, outputs));
        }
        if (warnings.isEmpty()) {
            applyMode(mode, false);
            return;
        }
        m_busy = false;
        setFlow(FlowNotice::Kind::Confirm, warnings.join(QLatin1Char(' ')), mode);
    });
}

void GpuPage::applyMode(Mode mode, bool force)
{
    m_busy = true;
    refreshBanner();
    m_control->setMode(mode, force, [this, mode](const SetModeResult &result) { finishSwitch(mode, result); });
}

void GpuPage::finishSwitch(Mode mode, const SetModeResult &result)
{
    m_busy = false;
    switch (result.status) {
    case SetModeResult::Status::Ok:
        m_rules->setDismissedFailure(m_state->lastFailure());
        clearFlow();
        m_state->reload();
        return;
    case SetModeResult::Status::NeedsConfirmation:
        setFlow(FlowNotice::Kind::Confirm, result.message, mode);
        return;
    case SetModeResult::Status::NotAuthorized: // the user closed the polkit dialog
        clearFlow();
        return;
    case SetModeResult::Status::Refused:
    case SetModeResult::Status::Failed:
        break;
    }
    const QString text = result.message.isEmpty()
        ? QCoreApplication::translate("GpuPage", "Could not switch the graphics mode") : result.message;
    setFlow(FlowNotice::Kind::Error, text, mode);
}

void GpuPage::setFlow(FlowNotice::Kind kind, const QString &text, Mode mode)
{
    m_flow = FlowNotice{kind, text, mode};
    refreshBanner();
}

void GpuPage::clearFlow()
{
    m_flow = FlowNotice();
    refreshBanner();
}

void GpuPage::dismissFailure()
{
    m_rules->setDismissedFailure(m_state->lastFailure());
    refreshBanner();
}

void GpuPage::bannerAction(const QString &actionId)
{
    if (actionId == QLatin1String("undo")) {
        if (!m_busy)
            applyMode(m_state->activeMode(), false);
    } else if (actionId == QLatin1String("restartNow")) {
        Session::requestReboot();
    } else if (actionId == QLatin1String("logoutNow")) {
        Session::requestLogout();
    } else if (actionId == QLatin1String("confirm")) {
        if (!m_busy && m_flow.kind == FlowNotice::Kind::Confirm) {
            const Mode mode = m_flow.mode;
            clearFlow();
            applyMode(mode, true);
        }
    } else if (actionId == QLatin1String("cancel")) {
        clearFlow();
    } else if (actionId == QLatin1String("dismiss")) {
        if (m_flow.kind != FlowNotice::Kind::None)
            clearFlow();
        else
            dismissFailure();
    }
}

} // namespace AginoGpu
