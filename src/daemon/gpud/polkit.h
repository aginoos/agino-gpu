// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace AginoGpu {

inline constexpr char kActionSetMode[] = "org.agino.gpu.set-mode";
inline constexpr char kActionListAllUsers[] = "org.agino.gpu.list-all-users";

enum class AuthResult { Authorized, Denied, Error };

// Asynchronous org.freedesktop.PolicyKit1.Authority.CheckAuthorization for a D-Bus caller.
// `done` runs on the event loop; `parent` owns the pending call.
void checkAuthorization(QObject *parent, const QString &busName, const QString &actionId,
                        bool allowInteraction, std::function<void(AuthResult)> done);

} // namespace AginoGpu
