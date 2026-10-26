// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace AginoGpu {

// design.md §5.2. Stored in the user DConfig org.agino.gpu (appid agino-gpu) as JSON strings;
// the DConfig plumbing lives in the plugins (P2). This file is the pure data part.
struct AppRule
{
    QString appId; // DAM application id / desktop id without ".desktop"
};

struct PathRule
{
    QString path;          // canonical absolute path of the executable / AppImage / script
    QString launcherEntry; // "agino-gpu-<hash>.desktop" or empty
};

struct RuleSet
{
    QList<AppRule> apps;
    QList<PathRule> paths;
    bool pauseOnBattery = true;
};

QByteArray ruleSetToJson(const RuleSet &rules);
std::optional<RuleSet> ruleSetFromJson(const QByteArray &json); // nullopt on malformed input

// "agino-gpu-" + first 16 hex chars of SHA-256 of the canonical path + ".desktop".
QString launcherEntryName(const QString &canonicalPath);
// Minimal desktop entry: Type=Application, Name=<name>, Exec=<path, quoted per the Desktop
// Entry spec>, PrefersNonDefaultGPU=true, X-Agino-Gpu-Rule=true, NoDisplay=false.
QByteArray launcherEntryContents(const QString &canonicalPath, const QString &name);

// DAM's appExtraEnvironments for one app is a list of "KEY=VALUE" strings. We mark our entries
// with kRuleMarker so removal only takes ours out (design.md §5.2).
inline constexpr char kRuleMarker[] = "AGINO_GPU_RULE=1";
// Returns `existing` with our entries replaced by `offloadEnv` + marker (order: existing
// foreign entries first, then ours). A foreign entry with the same KEY as one of ours is kept
// and ours for that KEY is skipped (the user's own setting wins).
QStringList mergeRuleEnvironment(const QStringList &existing, const QStringList &offloadEnv);
// Returns `existing` without our entries (only when the marker is present; without the marker
// nothing is ours and the list is returned unchanged).
QStringList removeRuleEnvironment(const QStringList &existing, const QStringList &offloadEnv);

// For the DAM patch (§5.3 item 1): resolve the program of an Exec= value to a canonical
// absolute path: first token after field codes are stripped, "env K=V ..." prefixes skipped,
// relative names looked up in `searchPath` (colon list), symlinks resolved via the file system.
// Returns empty when it cannot be resolved.
QString resolveExecProgram(const QString &exec, const QString &searchPath);

} // namespace AginoGpu
