// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpuservice.h"

#include "agino-gpu/configgen.h"
#include "agino-gpu/detect.h"
#include "agino-gpu/dgpuusers.h"
#include "agino-gpu/offloadenv.h"
#include "daemoncommon.h"
#include "polkit.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>

#include <optional>

namespace AginoGpu {

namespace {

const QString kErrorNotAuthorized = QStringLiteral("org.agino.Gpu1.Error.NotAuthorized");
const QString kErrorRefused = QStringLiteral("org.agino.Gpu1.Error.Refused");
const QString kErrorNeedsConfirmation = QStringLiteral("org.agino.Gpu1.Error.NeedsConfirmation");
const QString kErrorFailed = QStringLiteral("org.agino.Gpu1.Error.Failed");
const QString kErrorInvalidArgs = QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs");

constexpr char kMuxDedicatedValue[] = "0"; // asus-nb-wmi gpu_mux_mode: 0 = dedicated, 1 = hybrid
constexpr char kMuxHybridValue[] = "1";

QString levelName(CheckResult::Level level)
{
    switch (level) {
    case CheckResult::Level::Info:
        return QStringLiteral("info");
    case CheckResult::Level::Warning:
        return QStringLiteral("warning");
    case CheckResult::Level::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("info");
}

QString formatCheck(const CheckResult &r)
{
    return QStringLiteral("%1: %2").arg(r.code, r.message);
}

QStringList modeStrings(const QList<Mode> &modes)
{
    QStringList out;
    for (Mode m : modes)
        out.append(modeToString(m));
    return out;
}

} // namespace

GpuService::GpuService(Root root, int idleSeconds, QObject *parent)
    : QObject(parent)
    , m_root(std::move(root))
    , m_idle(new QTimer(this))
{
    m_idle->setSingleShot(true);
    m_idle->setInterval(idleSeconds * 1000);
    connect(m_idle, &QTimer::timeout, this, [this]() {
        if (m_pending > 0) {
            m_idle->start(); // a polkit check is still open
            return;
        }
        qInfo("idle, exiting");
        QCoreApplication::quit();
    });
    m_idle->start();
}

void GpuService::touch() const
{
    m_idle->start();
}

Mode GpuService::activeMode() const
{
    const auto state = readBootState(m_root);
    return state ? state->active : Mode::Hybrid;
}

QList<Mode> GpuService::offeredModes(const Hardware &hw) const
{
    return AginoGpu::offeredModes(hw, activeMode());
}

// ---- properties -------------------------------------------------------------------------

QString GpuService::hardwareClassProperty() const
{
    touch();
    return hardwareClassToString(detectHardware(m_root).hardwareClass);
}

QList<QVariantMap> GpuService::gpusProperty() const
{
    touch();
    const Hardware hw = detectHardware(m_root);
    QList<QVariantMap> out;
    for (const Gpu &g : hw.gpus) {
        QVariantMap m;
        m.insert(QStringLiteral("Id"), g.pciAddress);
        m.insert(QStringLiteral("Name"), g.name);
        m.insert(QStringLiteral("Vendor"), vendorName(g.vendor));
        m.insert(QStringLiteral("PciAddress"), g.pciAddress);
        m.insert(QStringLiteral("Driver"), g.driver);
        m.insert(QStringLiteral("Discrete"), g.discrete);
        m.insert(QStringLiteral("BootVga"), g.bootVga);
        m.insert(QStringLiteral("Removable"), g.removable);
        m.insert(QStringLiteral("ConnectedOutputs"), g.connectedOutputs);
        m.insert(QStringLiteral("RuntimePmSupported"), g.runtimePmSupported);
        out.append(m);
    }
    return out;
}

QStringList GpuService::supportedModesProperty() const
{
    touch();
    return modeStrings(offeredModes(detectHardware(m_root)));
}

QString GpuService::configuredModeProperty() const
{
    touch();
    return modeToString(readConfiguredMode(m_root));
}

QString GpuService::activeModeProperty() const
{
    touch();
    return modeToString(activeMode());
}

QString GpuService::pendingActionProperty() const
{
    touch();
    return pendingActionToString(pendingAction(readConfiguredMode(m_root), activeMode()));
}

QString GpuService::lastFailureProperty() const
{
    touch();
    return readLastFailure(m_root);
}

QStringList GpuService::offloadEnvironmentProperty() const
{
    touch();
    if (activeMode() != Mode::Hybrid)
        return {};
    const Hardware hw = detectHardware(m_root);
    const Gpu *dgpu = hw.discreteGpu();
    return dgpu ? offloadEnvironment(*dgpu, nvidiaGpuCount(hw)) : QStringList();
}

// ---- helpers ----------------------------------------------------------------------------

void GpuService::replyError(const QDBusMessage &message, const QString &name, const QString &text) const
{
    QDBusConnection::systemBus().send(message.createErrorReply(name, text));
}

void GpuService::replyUsers(const QDBusMessage &message, const QList<UserEntry> &users) const
{
    QDBusMessage r = message.createReply();
    r << QVariant::fromValue(users);
    QDBusConnection::systemBus().send(r);
}

void GpuService::emitPropertiesChanged(const QStringList &names) const
{
    QVariantMap changed;
    for (const QString &name : names) {
        if (name == QLatin1String("ConfiguredMode"))
            changed.insert(name, configuredModeProperty());
        else if (name == QLatin1String("PendingAction"))
            changed.insert(name, pendingActionProperty());
        else if (name == QLatin1String("LastFailure"))
            changed.insert(name, lastFailureProperty());
    }
    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/org/agino/Gpu1"), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    signal << QStringLiteral("org.agino.Gpu1") << changed << QStringList();
    QDBusConnection::systemBus().send(signal);
}

QList<CheckResult> GpuService::runChecks(const Hardware &hw, Mode target) const
{
    const Mode active = activeMode();
    if (target == active)
        return {}; // going back to what is running: nothing to warn about
    if (!hw.discreteGpu() && active == Mode::Integrated) {
        // The discrete GPU was removed from the bus by integrated mode; Hybrid brings it back.
        if (target == Mode::Hybrid)
            return {};
        return {CheckResult{CheckResult::Level::Error, QStringLiteral("unsupported-mode"),
                            QStringLiteral("only hybrid is possible while the discrete GPU is switched off")}};
    }
    return checkMode(hw, active, target, readCheckContext(m_root));
}

// ---- methods ----------------------------------------------------------------------------

QList<CheckEntry> GpuService::CheckMode(const QString &modeString)
{
    touch();
    const auto mode = modeFromString(modeString);
    if (!mode) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("unknown mode '%1'").arg(modeString));
        return {};
    }
    QList<CheckEntry> out;
    const Hardware hw = detectHardware(m_root);
    for (const CheckResult &r : runChecks(hw, *mode))
        out.append(CheckEntry{levelName(r.level), formatCheck(r)});
    return out;
}

void GpuService::SetMode(const QString &modeString, const QVariantMap &options)
{
    touch();
    const QDBusMessage msg = message();
    setDelayedReply(true);

    const auto mode = modeFromString(modeString);
    if (!mode) {
        replyError(msg, kErrorInvalidArgs, QStringLiteral("unknown mode '%1'").arg(modeString));
        return;
    }
    const bool force = options.value(QStringLiteral("force")).toBool();
    const Mode target = *mode;

    // Each SetMode may open an authentication dialog; refuse a second one while the first is
    // still waiting instead of stacking dialogs. Clients use a long call timeout (the user may
    // take a while to authenticate).
    if (m_setModeInFlight) {
        replyError(msg, kErrorFailed, QStringLiteral("another mode change is waiting for authorization"));
        return;
    }
    m_setModeInFlight = true;
    ++m_pending;
    checkAuthorization(this, msg.service(), QString::fromLatin1(kActionSetMode), true,
                       [this, msg, target, force](AuthResult result) {
                           --m_pending;
                           m_setModeInFlight = false;
                           touch();
                           if (result == AuthResult::Authorized) {
                               finishSetMode(msg, target, force);
                           } else if (result == AuthResult::Denied) {
                               qWarning("SetMode refused: not authorized (%s)", qPrintable(msg.service()));
                               replyError(msg, kErrorNotAuthorized, QStringLiteral("not authorized"));
                           } else {
                               qWarning("SetMode: polkit check failed");
                               replyError(msg, kErrorFailed, QStringLiteral("authorization check failed"));
                           }
                       });
}

void GpuService::finishSetMode(const QDBusMessage &message, Mode target, bool force)
{
    const Hardware hw = detectHardware(m_root);
    const QList<CheckResult> checks = runChecks(hw, target);

    QStringList errors;
    QStringList warnings;
    for (const CheckResult &r : checks) {
        if (r.level == CheckResult::Level::Error)
            errors.append(formatCheck(r));
        else if (r.level == CheckResult::Level::Warning)
            warnings.append(formatCheck(r));
    }
    if (!errors.isEmpty()) {
        qInfo("SetMode(%s) refused: %s", qPrintable(modeToString(target)), qPrintable(errors.join(QLatin1Char('\n'))));
        replyError(message, kErrorRefused, errors.join(QLatin1Char('\n')));
        return;
    }
    if (!warnings.isEmpty() && !force) {
        replyError(message, kErrorNeedsConfirmation, warnings.join(QLatin1Char('\n')));
        return;
    }

    const Mode previous = readConfiguredMode(m_root);
    // gpu_mux_mode reads back the position of this boot, not the one written for the next, so
    // it cannot be compared: write the position the mode needs every time (hybrid -> direct ->
    // hybrid in one boot must end at hybrid), and roll back to the previous mode's position.
    const bool hasMux = hw.mux.available && !hw.mux.sysfsPath.isEmpty();
    const auto muxValue = [](Mode mode) {
        return QByteArray(mode == Mode::MuxDedicated ? kMuxDedicatedValue : kMuxHybridValue);
    };
    const auto rollback = [&]() {
        QString e;
        if (!writeConfiguredMode(m_root, previous, &e))
            qWarning("SetMode rollback: %s", qPrintable(e));
        if (!applyGeneratedFiles(m_root, hw, previous, &e))
            qWarning("SetMode rollback: %s", qPrintable(e));
        if (hasMux && !writeSysfsValue(m_root, hw.mux.sysfsPath, muxValue(previous), &e))
            qWarning("SetMode rollback: %s", qPrintable(e));
    };

    QString err;
    if (!writeConfiguredMode(m_root, target, &err)) {
        qWarning("SetMode: %s", qPrintable(err));
        replyError(message, kErrorFailed, err);
        return;
    }
    if (!applyGeneratedFiles(m_root, hw, target, &err)) {
        qWarning("SetMode: %s", qPrintable(err));
        rollback();
        replyError(message, kErrorFailed, err);
        return;
    }

    // The MUX position is latched by the firmware at the next boot, so it is written now.
    if (hasMux && !writeSysfsValue(m_root, hw.mux.sysfsPath, muxValue(target), &err)) {
        qWarning("SetMode: %s", qPrintable(err));
        rollback();
        replyError(message, kErrorFailed, err);
        return;
    }

    qInfo("SetMode: configured mode %s -> %s", qPrintable(modeToString(previous)), qPrintable(modeToString(target)));
    QDBusConnection::systemBus().send(message.createReply());
    emitPropertiesChanged({QStringLiteral("ConfiguredMode"), QStringLiteral("PendingAction")});
}

void GpuService::GetDgpuUsers()
{
    touch();
    const QDBusMessage msg = message();
    setDelayedReply(true);

    const QDBusReply<uint> uidReply = QDBusConnection::systemBus().interface()->serviceUid(msg.service());
    if (!uidReply.isValid()) {
        replyError(msg, kErrorFailed, QStringLiteral("cannot determine the caller"));
        return;
    }
    const quint32 callerUid = uidReply.value();

    ++m_pending;
    checkAuthorization(this, msg.service(), QString::fromLatin1(kActionListAllUsers), false,
                       [this, msg, callerUid](AuthResult result) {
                           --m_pending;
                           touch();
                           const Hardware hw = detectHardware(m_root);
                           const Gpu *dgpu = hw.discreteGpu();
                           QList<UserEntry> users;
                           if (dgpu) {
                               const std::optional<quint32> only = result == AuthResult::Authorized
                                   ? std::nullopt
                                   : std::optional<quint32>(callerUid);
                               for (const GpuUser &u : findGpuUsers(m_root, *dgpu, only))
                                   users.append(UserEntry{u.pid, u.uid, u.name});
                           }
                           replyUsers(msg, users);
                       });
}

} // namespace AginoGpu
