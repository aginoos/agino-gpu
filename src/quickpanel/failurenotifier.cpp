// SPDX-License-Identifier: GPL-3.0-or-later
#include "failurenotifier.h"

#include "agino-gpu-client/gpustate.h"
#include "agino-gpu-client/rulestore.h"
#include "agino-gpu-client/texts.h"
#include "controlcenter.h"
#include "qptexts.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QVariantMap>

namespace AginoGpu {

namespace {

const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
const QString kInterface = QStringLiteral("org.freedesktop.Notifications");
const QString kDefaultAction = QStringLiteral("default");

} // namespace

FailureNotifier::FailureNotifier(GpuState *state, RuleStore *rules, QObject *parent)
    : QObject(parent)
    , m_state(state)
    , m_rules(rules)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kService, kPath, kInterface, QStringLiteral("ActionInvoked"), this,
                SLOT(onActionInvoked(uint, QString)));
    bus.connect(kService, kPath, kInterface, QStringLiteral("NotificationClosed"), this,
                SLOT(onClosed(uint, uint)));
}

void FailureNotifier::notifyIfNeeded()
{
    const QString failure = m_state->lastFailure();
    if (failure.isEmpty() || failure == m_rules->dismissedFailure() || failure == m_failure)
        return;
    send(failure);
}

void FailureNotifier::send(const QString &failure)
{
    m_failure = failure;
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Notify"));
    call << QStringLiteral("agino-gpu") << uint(0) << QStringLiteral("video-card")
         << QpTexts::switchFailedTitle() << Texts::failureBanner(m_state->configuredMode())
         << QStringList { kDefaultAction, Texts::details() } << QVariantMap() << -1;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<uint> reply = *w;
        if (reply.isError())
            qWarning() << "agino-gpu: cannot send the failure notification:" << reply.error().message();
        else
            m_id = reply.value();
        w->deleteLater();
    });
}

void FailureNotifier::onActionInvoked(uint id, const QString &actionKey)
{
    if (id != m_id || id == 0 || actionKey != kDefaultAction)
        return;
    openControlCenterGpuPage();
    dismiss();
}

void FailureNotifier::onClosed(uint id, uint reason)
{
    Q_UNUSED(reason);
    if (id == m_id && id != 0)
        dismiss();
}

void FailureNotifier::dismiss()
{
    if (!m_failure.isEmpty())
        m_rules->setDismissedFailure(m_failure);
    m_id = 0;
}

} // namespace AginoGpu
