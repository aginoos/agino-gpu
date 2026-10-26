// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "agino-gpu/mode.h"
#include "agino-gpu/root.h"
#include "agino-gpu/types.h"
#include "dbustypes.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

namespace AginoGpu {

// org.agino.Gpu1 on /org/agino/Gpu1 (design.md §6.2). Property getters and methods are exported
// through ExportAllContents; everything that is not a public slot or property stays private.
class GpuService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.agino.Gpu1")
    Q_PROPERTY(QString HardwareClass READ hardwareClassProperty)
    Q_PROPERTY(QList<QVariantMap> Gpus READ gpusProperty)
    Q_PROPERTY(QStringList SupportedModes READ supportedModesProperty)
    Q_PROPERTY(QString ConfiguredMode READ configuredModeProperty)
    Q_PROPERTY(QString ActiveMode READ activeModeProperty)
    Q_PROPERTY(QString PendingAction READ pendingActionProperty)
    Q_PROPERTY(QString LastFailure READ lastFailureProperty)
    Q_PROPERTY(QStringList OffloadEnvironment READ offloadEnvironmentProperty)

public:
    explicit GpuService(Root root, int idleSeconds, QObject *parent = nullptr);

    QString hardwareClassProperty() const;
    QList<QVariantMap> gpusProperty() const;
    QStringList supportedModesProperty() const;
    QString configuredModeProperty() const;
    QString activeModeProperty() const;
    QString pendingActionProperty() const;
    QString lastFailureProperty() const;
    QStringList offloadEnvironmentProperty() const;

public Q_SLOTS:
    QList<CheckEntry> CheckMode(const QString &mode);
    void SetMode(const QString &mode, const QVariantMap &options);
    void GetDgpuUsers();

private:
    void touch() const;
    Mode activeMode() const;
    // Modes the UI may offer; adds the integrated/hybrid pair when the discrete GPU is already
    // gone because the active mode is integrated.
    QList<Mode> offeredModes(const Hardware &hw) const;
    QList<CheckResult> runChecks(const Hardware &hw, Mode target) const;
    void finishSetMode(const QDBusMessage &message, Mode target, bool force);
    void emitPropertiesChanged(const QStringList &names) const;
    void replyUsers(const QDBusMessage &message, const QList<UserEntry> &users) const;
    void replyError(const QDBusMessage &message, const QString &name, const QString &text) const;

    Root m_root;
    QTimer *m_idle = nullptr;
    int m_pending = 0;
};

} // namespace AginoGpu
