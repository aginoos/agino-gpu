// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/gpucontrol.h"

#include "agino-gpu/dbustypes.h"
#include "agino-gpu/mode.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>

namespace AginoGpu {

namespace {

const QString kService = QStringLiteral("org.agino.Gpu1");
const QString kPath = QStringLiteral("/org/agino/Gpu1");
const QString kInterface = QStringLiteral("org.agino.Gpu1");

constexpr int kDefaultTimeoutMs = 25000;
// SetMode waits for the user in the polkit dialog.
constexpr int kSetModeTimeoutMs = 10 * 60 * 1000;

QDBusMessage methodCall(const QString &method)
{
    return QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
}

// Runs `handler` with the finished call unless `context` is gone by then.
template<typename Handler>
void whenFinished(QObject *context, const QDBusPendingCall &call, Handler handler)
{
    auto *watcher = new QDBusPendingCallWatcher(call, context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
                     [watcher, handler](QDBusPendingCallWatcher *) {
                         watcher->deleteLater();
                         handler(*watcher);
                     });
}

SetModeResult::Status statusForError(const QString &name)
{
    if (name == QLatin1String("org.agino.Gpu1.Error.NeedsConfirmation"))
        return SetModeResult::Status::NeedsConfirmation;
    if (name == QLatin1String("org.agino.Gpu1.Error.NotAuthorized"))
        return SetModeResult::Status::NotAuthorized;
    if (name == QLatin1String("org.agino.Gpu1.Error.Refused"))
        return SetModeResult::Status::Refused;
    return SetModeResult::Status::Failed;
}

} // namespace

GpuControl::GpuControl(QObject *parent)
    : QObject(parent)
{
    registerDBusTypes();
}

CheckItem GpuControl::parseCheckEntry(const QString &level, const QString &text)
{
    CheckItem item;
    if (level == QLatin1String("error"))
        item.level = CheckItem::Level::Error;
    else if (level == QLatin1String("warning"))
        item.level = CheckItem::Level::Warning;
    // "code: message"
    const qsizetype sep = text.indexOf(QLatin1String(": "));
    if (sep > 0) {
        item.code = text.left(sep);
        item.message = text.mid(sep + 2);
    } else {
        item.message = text;
    }
    return item;
}

void GpuControl::checkMode(Mode mode, CheckCallback callback)
{
    QDBusMessage msg = methodCall(QStringLiteral("CheckMode"));
    msg << modeToString(mode);
    whenFinished(this, QDBusConnection::systemBus().asyncCall(msg, kDefaultTimeoutMs),
                 [callback](const QDBusPendingCall &call) {
                     QDBusPendingReply<QList<CheckEntry>> reply(call);
                     QList<CheckItem> items;
                     if (reply.isValid()) {
                         const QList<CheckEntry> entries = reply.value();
                         for (const CheckEntry &e : entries)
                             items << parseCheckEntry(e.level, e.text);
                     } else {
                         qWarning("agino-gpu: CheckMode failed: %s",
                                  qPrintable(reply.error().message()));
                     }
                     callback(items);
                 });
}

void GpuControl::setMode(Mode mode, bool force, SetModeCallback callback)
{
    QDBusMessage msg = methodCall(QStringLiteral("SetMode"));
    msg.setInteractiveAuthorizationAllowed(true);
    msg << modeToString(mode) << QVariantMap{{QStringLiteral("force"), force}};
    whenFinished(this, QDBusConnection::systemBus().asyncCall(msg, kSetModeTimeoutMs),
                 [callback](const QDBusPendingCall &call) {
                     QDBusPendingReply<> reply(call);
                     SetModeResult result;
                     if (reply.isError()) {
                         result.status = statusForError(reply.error().name());
                         result.message = reply.error().message();
                     } else {
                         result.status = SetModeResult::Status::Ok;
                     }
                     callback(result);
                 });
}

void GpuControl::dgpuUsers(UsersCallback callback)
{
    whenFinished(this, QDBusConnection::systemBus().asyncCall(methodCall(QStringLiteral("GetDgpuUsers")),
                                                               kDefaultTimeoutMs),
                 [callback](const QDBusPendingCall &call) {
                     QDBusPendingReply<QList<UserEntry>> reply(call);
                     QList<GpuUser> users;
                     if (reply.isValid()) {
                         const QList<UserEntry> entries = reply.value();
                         for (const UserEntry &e : entries)
                             users << GpuUser{e.pid, e.uid, e.name};
                     }
                     callback(users);
                 });
}

} // namespace AginoGpu
