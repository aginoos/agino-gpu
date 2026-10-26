// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/types.h"

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace AginoGpu {

// What the last user action left behind: a refused or failed switch, or warnings the user has
// to confirm before the switch goes ahead.
struct FlowNotice
{
    enum class Kind { None, Error, Confirm };
    Kind kind = Kind::None;
    QString text;
    Mode mode = Mode::Hybrid; // the mode awaiting confirmation
};

struct BannerInput
{
    Mode configured = Mode::Hybrid;
    std::optional<Mode> failedMode;
    Mode active = Mode::Hybrid;
    PendingAction pending = PendingAction::None;
    QString lastFailure;
    bool failureDismissed = false;
    FlowNotice flow;
    QStringList dgpuOutputs;     // connectors on the discrete GPU that have a display
    bool integratedOffered = false;
};

// The single banner shown above the page (same rules as the quick panel): the current flow
// notice, else a pending switch, else the last boot failure, else the external-display note.
// Keys: visible, kind ("info" | "warning" | "error"), text, detail, actions
// (list of {id, label, primary}). Empty map with visible=false when there is nothing to show.
QVariantMap buildBanner(const BannerInput &input);

} // namespace AginoGpu
