// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/dgpuusers.h"
#include "agino-gpu/types.h"

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

namespace AginoGpu {

// One CheckMode result (design.md §3.3). `code` is the stable id the UI translates.
struct CheckItem
{
    enum class Level { Info, Warning, Error };
    Level level = Level::Info;
    QString code;
    QString message; // English fallback from the daemon
};

struct SetModeResult
{
    enum class Status { Ok, NeedsConfirmation, NotAuthorized, Refused, Failed };
    Status status = Status::Failed;
    QString message;
};

// Calls into org.agino.Gpu1 (design.md §6.2). Every call is asynchronous; callbacks run in the
// caller's thread and are dropped when this object is destroyed first. The daemon is D-Bus
// activated, so calls work whether or not it is running.
class GpuControl : public QObject
{
    Q_OBJECT
public:
    using CheckCallback = std::function<void(const QList<CheckItem> &)>;
    using SetModeCallback = std::function<void(const SetModeResult &)>;
    using UsersCallback = std::function<void(const QList<GpuUser> &)>;

    explicit GpuControl(QObject *parent = nullptr);

    // Empty list when the daemon cannot be reached (the UI then offers the switch unchecked
    // and SetMode reports the failure).
    void checkMode(Mode mode, CheckCallback callback);
    // force: the user confirmed the warnings CheckMode returned. The call waits for the polkit
    // dialog, so its timeout is long.
    void setMode(Mode mode, bool force, SetModeCallback callback);
    // Processes using the discrete GPU: the caller's own, or everyone's when polkit already
    // allows it (org.agino.gpu.list-all-users).
    void dgpuUsers(UsersCallback callback);

    static CheckItem parseCheckEntry(const QString &level, const QString &text);
};

} // namespace AginoGpu
