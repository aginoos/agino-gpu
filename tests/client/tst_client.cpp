// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu-client/damsync.h"
#include "agino-gpu-client/gpucontrol.h"
#include "agino-gpu-client/gpustate.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace AginoGpu;

class TstClient : public QObject
{
    Q_OBJECT

private:
    static const QStringList &nvEnv()
    {
        static const QStringList env{QStringLiteral("__NV_PRIME_RENDER_OFFLOAD=1"),
                                     QStringLiteral("__GLX_VENDOR_LIBRARY_NAME=nvidia")};
        return env;
    }

    // Copies a fixture tree into a writable temporary root.
    static bool copyTree(const QString &from, const QString &to)
    {
        QDir src(from);
        if (!QDir().mkpath(to))
            return false;
        const QFileInfoList entries = src.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
        for (const QFileInfo &e : entries) {
            const QString target = to + QLatin1Char('/') + e.fileName();
            if (e.isSymLink()) {
                if (!QFile::link(e.readSymLink(), target)) // keep relative links relative
                    return false;
            } else if (e.isDir()) {
                if (!copyTree(e.filePath(), target))
                    return false;
            } else if (!QFile::copy(e.filePath(), target)) {
                return false;
            }
        }
        return true;
    }

private Q_SLOTS:
    void planAddsNewApps()
    {
        const QList<DamUpdate> plan = planDamSync({}, {}, {QStringLiteral("steam"), QStringLiteral("blender")}, nvEnv());
        QCOMPARE(plan.size(), 2);
        QCOMPARE(plan.at(0), (DamUpdate{QStringLiteral("blender"), {}, nvEnv()}));
        QCOMPARE(plan.at(1), (DamUpdate{QStringLiteral("steam"), {}, nvEnv()}));
    }

    void planSkipsCurrentApps()
    {
        QVERIFY(planDamSync({QStringLiteral("steam")}, nvEnv(), {QStringLiteral("steam")}, nvEnv()).isEmpty());
    }

    void planRemovesDroppedApps()
    {
        const QList<DamUpdate> plan =
            planDamSync({QStringLiteral("steam"), QStringLiteral("blender")}, nvEnv(), {QStringLiteral("steam")}, nvEnv());
        QCOMPARE(plan.size(), 1);
        QCOMPARE(plan.at(0), (DamUpdate{QStringLiteral("blender"), nvEnv(), {}}));
    }

    void planEmptyEnvRemovesEverything()
    {
        // paused on battery / not hybrid: desired env is empty, every applied app is cleaned
        const QList<DamUpdate> plan = planDamSync({QStringLiteral("steam")}, nvEnv(), {QStringLiteral("steam")}, {});
        QCOMPARE(plan.size(), 1);
        QCOMPARE(plan.at(0), (DamUpdate{QStringLiteral("steam"), nvEnv(), {}}));
    }

    void planEnvChangeRewritesAll()
    {
        const QStringList newEnv{QStringLiteral("DRI_PRIME=pci-0000_03_00_0")};
        const QList<DamUpdate> plan = planDamSync({QStringLiteral("steam")}, nvEnv(), {QStringLiteral("steam")}, newEnv);
        QCOMPARE(plan.size(), 1);
        QCOMPARE(plan.at(0), (DamUpdate{QStringLiteral("steam"), nvEnv(), newEnv}));
    }

    void inForce()
    {
        QVERIFY(rulesInForce(true, true, true, false));
        QVERIFY(rulesInForce(true, true, false, true));
        QVERIFY(!rulesInForce(true, true, true, true));
        QVERIFY(!rulesInForce(false, true, false, false));
        QVERIFY(!rulesInForce(true, false, false, false));
    }

    void parseCheck()
    {
        const CheckItem w = GpuControl::parseCheckEntry(QStringLiteral("warning"),
                                                        QStringLiteral("on-battery: Running on battery"));
        QCOMPARE(w.level, CheckItem::Level::Warning);
        QCOMPARE(w.code, QStringLiteral("on-battery"));
        QCOMPARE(w.message, QStringLiteral("Running on battery"));

        const CheckItem e = GpuControl::parseCheckEntry(QStringLiteral("error"), QStringLiteral("no code"));
        QCOMPARE(e.level, CheckItem::Level::Error);
        QVERIFY(e.code.isEmpty());
        QCOMPARE(e.message, QStringLiteral("no code"));

        QCOMPARE(GpuControl::parseCheckEntry(QStringLiteral("info"), QString()).level, CheckItem::Level::Info);
    }

    void stateSingleGpu()
    {
        const GpuState state(Root(QStringLiteral(AGINO_GPU_FIXTURES "/single")));
        QVERIFY(!state.isMultiGpu());
        QCOMPARE(state.dgpuStatus(), DgpuStatus::None);
    }

    void stateFollowsModeFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyTree(QStringLiteral(AGINO_GPU_FIXTURES "/hybrid-nvidia"), dir.path()));
        const Root root(dir.path());
        QVERIFY(root.writeFile(QStringLiteral("/run/agino-gpu/state"), "active=hybrid\nfailure=\n"));
        QVERIFY(root.writeFile(QStringLiteral("/etc/agino-gpu/mode"), "hybrid\n"));

        GpuState state(root);
        QVERIFY(state.isMultiGpu());
        QCOMPARE(state.configuredMode(), Mode::Hybrid);
        QCOMPARE(state.pendingAction(), PendingAction::None);

        QSignalSpy spy(&state, &GpuState::changed);
        QVERIFY(root.writeFile(QStringLiteral("/etc/agino-gpu/mode"), "integrated\n"));
        QVERIFY(spy.wait(5000));
        QCOMPARE(state.configuredMode(), Mode::Integrated);
        QCOMPARE(state.pendingAction(), PendingAction::Reboot);
    }
};

QTEST_GUILESS_MAIN(TstClient)
#include "tst_client.moc"
