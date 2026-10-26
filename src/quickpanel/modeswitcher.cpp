// SPDX-License-Identifier: GPL-3.0-or-later
#include "modeswitcher.h"

#include "agino-gpu-client/texts.h"
#include "qptexts.h"

namespace AginoGpu {

ModeSwitcher::ModeSwitcher(GpuState *state, GpuControl *control, QObject *parent)
    : QObject(parent)
    , m_state(state)
    , m_control(control)
{
}

void ModeSwitcher::select(Mode mode)
{
    if (m_busy || mode == m_state->configuredMode())
        return;
    clearNotice();
    m_busy = true;
    m_control->checkMode(mode, [this, mode](const QList<CheckItem> &items) {
        QStringList errors;
        QStringList warnings;
        const Gpu *gpu = m_state->discreteGpu();
        const QStringList outputs = gpu ? gpu->connectedOutputs : QStringList();
        for (const CheckItem &item : items) {
            if (item.level == CheckItem::Level::Error)
                errors << Texts::checkText(item, outputs);
            else if (item.level == CheckItem::Level::Warning)
                warnings << Texts::checkText(item, outputs);
        }
        m_busy = false;
        if (!errors.isEmpty())
            setNotice(errors.join(QLatin1Char('\n')), false, mode);
        else if (!warnings.isEmpty())
            setNotice(warnings.join(QLatin1Char('\n')), true, mode);
        else
            request(mode, false);
    });
}

void ModeSwitcher::confirm()
{
    if (m_busy || !m_notice.needsConfirm)
        return;
    const Mode mode = m_target;
    clearNotice();
    request(mode, true);
}

void ModeSwitcher::undo()
{
    if (m_busy)
        return;
    clearNotice();
    request(m_state->activeMode(), false);
}

void ModeSwitcher::clearNotice()
{
    if (m_notice.isEmpty())
        return;
    m_notice = Notice();
    Q_EMIT changed();
}

void ModeSwitcher::request(Mode mode, bool force)
{
    m_busy = true;
    m_control->setMode(mode, force, [this, mode](const SetModeResult &result) { finish(result, mode); });
}

void ModeSwitcher::finish(const SetModeResult &result, Mode mode)
{
    m_busy = false;
    using Status = SetModeResult::Status;
    switch (result.status) {
    case Status::Ok:
    case Status::NotAuthorized:
        m_state->reload();
        Q_EMIT changed(); // radio list snaps back to the configured mode
        break;
    case Status::NeedsConfirmation:
        setNotice(result.message, true, mode);
        break;
    case Status::Refused:
    case Status::Failed:
        setNotice(result.message.isEmpty() ? QpTexts::genericFailure() : result.message, false, mode);
        break;
    }
}

void ModeSwitcher::setNotice(const QString &text, bool needsConfirm, Mode mode)
{
    m_notice.text = text;
    m_notice.needsConfirm = needsConfirm;
    m_target = mode;
    Q_EMIT changed();
}

} // namespace AginoGpu
