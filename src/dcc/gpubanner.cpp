// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpubanner.h"

#include "agino-gpu-client/gpucontrol.h"
#include "agino-gpu-client/texts.h"

#include <QCoreApplication>
#include <QVariantList>

namespace AginoGpu {

namespace {

QVariantMap action(const QString &id, const QString &label, bool primary = false)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("label"), label}, {QStringLiteral("primary"), primary}};
}

QVariantMap banner(const QString &kind, const QString &text, const QVariantList &actions,
                   const QString &detail = QString())
{
    return {{QStringLiteral("visible"), true},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("text"), text},
            {QStringLiteral("detail"), detail},
            {QStringLiteral("actions"), actions}};
}

QVariantMap flowBanner(const FlowNotice &flow)
{
    if (flow.kind == FlowNotice::Kind::Confirm) {
        return banner(QStringLiteral("warning"), flow.text,
                      {action(QStringLiteral("cancel"), QCoreApplication::translate("GpuBanner", "Cancel")),
                       action(QStringLiteral("confirm"), QCoreApplication::translate("GpuBanner", "Switch anyway"), true)});
    }
    return banner(QStringLiteral("error"), flow.text, {action(QStringLiteral("dismiss"), QString())});
}

QVariantMap pendingBanner(const BannerInput &in)
{
    const bool logout = in.pending == PendingAction::Logout;
    return banner(QStringLiteral("warning"), Texts::pendingBanner(in.configured, in.pending),
                  {action(QStringLiteral("undo"), Texts::undo()),
                   logout ? action(QStringLiteral("logoutNow"), Texts::logoutNow(), true)
                          : action(QStringLiteral("restartNow"), Texts::restartNow(), true)});
}

} // namespace

QVariantMap buildBanner(const BannerInput &in)
{
    if (in.flow.kind != FlowNotice::Kind::None)
        return flowBanner(in.flow);
    if (in.pending != PendingAction::None)
        return pendingBanner(in);
    if (!in.lastFailure.isEmpty() && !in.failureDismissed) {
        return banner(QStringLiteral("error"), Texts::failureBanner(in.configured),
                      {action(QStringLiteral("details"), Texts::details()), action(QStringLiteral("dismiss"), QString())},
                      in.lastFailure);
    }
    if (in.active == Mode::Hybrid && in.integratedOffered && !in.dgpuOutputs.isEmpty()) {
        CheckItem item;
        item.level = CheckItem::Level::Warning;
        item.code = QStringLiteral("external-display-on-dgpu");
        return banner(QStringLiteral("warning"), Texts::checkText(item, in.dgpuOutputs), {});
    }
    return {{QStringLiteral("visible"), false}};
}

} // namespace AginoGpu
