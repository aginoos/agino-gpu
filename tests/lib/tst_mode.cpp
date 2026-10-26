// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/configgen.h"
#include "agino-gpu/detect.h"
#include "agino-gpu/mode.h"

#include <QtTest>

using namespace AginoGpu;

namespace {

Hardware detect(const char *name)
{
    return detectHardware(
        Root(QString::fromUtf8(AGINO_GPU_FIXTURES) + QLatin1Char('/') + QString::fromUtf8(name)));
}

QStringList codes(const QList<CheckResult> &results)
{
    QStringList out;
    for (const CheckResult &r : results)
        out.append(r.code);
    return out;
}

const CheckResult *find(const QList<CheckResult> &results, const QString &code)
{
    for (const CheckResult &r : results) {
        if (r.code == code)
            return &r;
    }
    return nullptr;
}

const QList<Mode> kAllModes = {Mode::Hybrid, Mode::Integrated, Mode::Dedicated, Mode::MuxDedicated};

} // namespace

class TstMode : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void modeStrings()
    {
        QCOMPARE(modeToString(Mode::Hybrid), QStringLiteral("hybrid"));
        QCOMPARE(modeToString(Mode::Integrated), QStringLiteral("integrated"));
        QCOMPARE(modeToString(Mode::Dedicated), QStringLiteral("dedicated"));
        QCOMPARE(modeToString(Mode::MuxDedicated), QStringLiteral("mux-dedicated"));
        for (Mode m : kAllModes)
            QCOMPARE(modeFromString(modeToString(m)), std::optional<Mode>(m));
        QCOMPARE(modeFromString(QStringLiteral(" dedicated\n")), std::optional<Mode>(Mode::Dedicated));
        QVERIFY(!modeFromString(QStringLiteral("bogus")).has_value());
        QVERIFY(!modeFromString(QString()).has_value());
        QCOMPARE(pendingActionToString(PendingAction::None), QStringLiteral("none"));
        QCOMPARE(pendingActionToString(PendingAction::Logout), QStringLiteral("logout"));
        QCOMPARE(pendingActionToString(PendingAction::Reboot), QStringLiteral("reboot"));
    }

    void supportedModes_data()
    {
        QTest::addColumn<QString>("fixture");
        QTest::addColumn<QList<Mode>>("expected");
        QTest::newRow("single") << "single" << QList<Mode>{};
        QTest::newRow("hybrid") << "hybrid-nvidia"
                                << QList<Mode>{Mode::Hybrid, Mode::Integrated, Mode::Dedicated};
        QTest::newRow("hybrid-amd") << "hybrid-amd"
                                    << QList<Mode>{Mode::Hybrid, Mode::Integrated, Mode::Dedicated};
        QTest::newRow("mux-hybrid") << "hybrid-nvidia-mux" << kAllModes;
        QTest::newRow("mux-dedicated")
            << "mux-dedicated-active" << QList<Mode>{Mode::Hybrid, Mode::MuxDedicated};
        QTest::newRow("desktop-igpu")
            << "desktop-igpu-display" << QList<Mode>{Mode::Hybrid, Mode::Integrated};
        QTest::newRow("desktop-dgpu") << "desktop-dgpu-display" << QList<Mode>{Mode::Hybrid};
        QTest::newRow("egpu") << "egpu" << QList<Mode>{Mode::Hybrid};
    }
    void supportedModes()
    {
        QFETCH(QString, fixture);
        QFETCH(QList<Mode>, expected);
        const Hardware hw = detect(fixture.toLatin1().constData());
        QCOMPARE(AginoGpu::supportedModes(hw), expected);
    }

    void pendingActionMatrix()
    {
        for (Mode m : kAllModes)
            QCOMPARE(pendingAction(m, m), PendingAction::None);
        // Dedicated <-> Hybrid: logout
        QCOMPARE(pendingAction(Mode::Dedicated, Mode::Hybrid), PendingAction::Logout);
        QCOMPARE(pendingAction(Mode::Hybrid, Mode::Dedicated), PendingAction::Logout);
        // Integrated and MuxDedicated are always a reboot
        for (Mode m : kAllModes) {
            if (m != Mode::Integrated) {
                QCOMPARE(pendingAction(Mode::Integrated, m), m == Mode::Integrated ? PendingAction::None : PendingAction::Reboot);
                QCOMPARE(pendingAction(m, Mode::Integrated), m == Mode::Integrated ? PendingAction::None : PendingAction::Reboot);
            }
            if (m != Mode::MuxDedicated) {
                QCOMPARE(pendingAction(Mode::MuxDedicated, m), PendingAction::Reboot);
                QCOMPARE(pendingAction(m, Mode::MuxDedicated), PendingAction::Reboot);
            }
        }
    }

    void configuredModeRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const Root root(dir.path());
        QCOMPARE(readConfiguredMode(root), Mode::Hybrid); // missing file
        for (Mode m : kAllModes) {
            QString error;
            QVERIFY2(writeConfiguredMode(root, m, &error), qPrintable(error));
            QCOMPARE(readConfiguredMode(root), m);
        }
        QCOMPARE(root.readTrimmed(QString::fromLatin1(kModePath)), std::optional<QString>(QStringLiteral("mux-dedicated")));
        QVERIFY(root.writeFile(QString::fromLatin1(kModePath), "garbage\n"));
        QCOMPARE(readConfiguredMode(root), Mode::Hybrid); // invalid
    }

    void bootStateRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const Root root(dir.path());
        QVERIFY(!readBootState(root).has_value());

        QVERIFY(writeBootState(root, BootState{Mode::Dedicated, QString()}));
        auto state = readBootState(root);
        QVERIFY(state.has_value());
        QCOMPARE(state->active, Mode::Dedicated);
        QVERIFY(state->failure.isEmpty());

        QVERIFY(writeBootState(root, BootState{Mode::Hybrid, QStringLiteral("Xorg failed=1\nsecond line")}));
        state = readBootState(root);
        QVERIFY(state.has_value());
        QCOMPARE(state->active, Mode::Hybrid);
        QCOMPARE(state->failure, QStringLiteral("Xorg failed=1 second line"));
    }

    void lastFailure()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const Root root(dir.path());
        QVERIFY(readLastFailure(root).isEmpty());
        QVERIFY(root.writeFile(QString::fromLatin1(kLastFailurePath), "X server did not start\n"));
        QCOMPARE(readLastFailure(root), QStringLiteral("X server did not start"));
    }

    void unsupportedMode()
    {
        const CheckContext ctx;
        const Hardware single = detect("single");
        QCOMPARE(codes(checkMode(single, Mode::Hybrid, Mode::Dedicated, ctx)), QStringList{QStringLiteral("unsupported-mode")});
        const auto results = checkMode(detect("hybrid-nvidia"), Mode::Hybrid, Mode::MuxDedicated, ctx);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).code, QStringLiteral("unsupported-mode"));
        QCOMPARE(results.at(0).level, CheckResult::Level::Error);
        QVERIFY(!results.at(0).message.isEmpty());
    }

    void cleanSwitchHasNoResults()
    {
        const CheckContext ctx;
        const Hardware hw = detect("hybrid-nvidia");
        QVERIFY(checkMode(hw, Mode::Hybrid, Mode::Integrated, ctx).isEmpty());
        QVERIFY(checkMode(hw, Mode::Hybrid, Mode::Dedicated, ctx).isEmpty());
        QVERIFY(checkMode(hw, Mode::Dedicated, Mode::Hybrid, ctx).isEmpty());
    }

    void externalDisplayOnDgpu()
    {
        Hardware hw = detect("hybrid-nvidia");
        hw.gpus[hw.discreteIndex].connectedOutputs = {QStringLiteral("HDMI-A-1")};
        const auto results = checkMode(hw, Mode::Hybrid, Mode::Integrated, CheckContext());
        QCOMPARE(codes(results), QStringList{QStringLiteral("external-display-on-dgpu")});
        QCOMPARE(results.at(0).level, CheckResult::Level::Warning);
        // only relevant for integrated
        QVERIFY(checkMode(hw, Mode::Hybrid, Mode::Dedicated, CheckContext()).isEmpty());
    }

    void panelOnDgpu()
    {
        const Hardware hw = detect("mux-dedicated-active");
        const auto results = checkMode(hw, Mode::MuxDedicated, Mode::Integrated, CheckContext());
        QCOMPARE(codes(results), QStringList{QStringLiteral("panel-on-dgpu")});
        QCOMPARE(results.at(0).level, CheckResult::Level::Error);
        // going back to hybrid is fine
        QVERIFY(checkMode(hw, Mode::MuxDedicated, Mode::Hybrid, CheckContext()).isEmpty());
    }

    void desktopDisplayOnDgpu()
    {
        const auto results = checkMode(detect("desktop-dgpu-display"), Mode::Hybrid, Mode::Integrated, CheckContext());
        QCOMPARE(codes(results), QStringList{QStringLiteral("desktop-display-on-dgpu")});
        QCOMPARE(results.at(0).level, CheckResult::Level::Error);
        // display on the iGPU: integrated is allowed
        QVERIFY(checkMode(detect("desktop-igpu-display"), Mode::Hybrid, Mode::Integrated, CheckContext()).isEmpty());
    }

    void egpuRefusals()
    {
        const Hardware hw = detect("egpu");
        for (Mode m : {Mode::Integrated, Mode::Dedicated}) {
            const auto results = checkMode(hw, Mode::Hybrid, m, CheckContext());
            QCOMPARE(codes(results), QStringList{QStringLiteral("egpu")});
            QCOMPARE(results.at(0).level, CheckResult::Level::Error);
        }
        QVERIFY(checkMode(hw, Mode::Hybrid, Mode::Hybrid, CheckContext()).isEmpty());
        QCOMPARE(codes(checkMode(hw, Mode::Hybrid, Mode::MuxDedicated, CheckContext())), QStringList{QStringLiteral("unsupported-mode")});
    }

    void secureBootAndBattery()
    {
        const Hardware hw = detect("hybrid-nvidia-mux");
        CheckContext ctx;
        ctx.onBattery = true;
        ctx.secureBootEnabled = true;
        ctx.nvidiaModuleSigned = false;
        for (Mode m : {Mode::Dedicated, Mode::MuxDedicated}) {
            const auto results = checkMode(hw, Mode::Hybrid, m, ctx);
            QCOMPARE(codes(results), (QStringList{QStringLiteral("secure-boot-unsigned"), QStringLiteral("on-battery")}));
            QCOMPARE(find(results, QStringLiteral("secure-boot-unsigned"))->level, CheckResult::Level::Warning);
            QCOMPARE(find(results, QStringLiteral("on-battery"))->level, CheckResult::Level::Info);
        }
        // not for hybrid / integrated
        QVERIFY(checkMode(hw, Mode::Dedicated, Mode::Hybrid, ctx).isEmpty());
        QVERIFY(checkMode(hw, Mode::Hybrid, Mode::Integrated, ctx).isEmpty());

        // signed module or Secure Boot off: no warning
        ctx.nvidiaModuleSigned = true;
        QCOMPARE(codes(checkMode(hw, Mode::Hybrid, Mode::Dedicated, ctx)), QStringList{QStringLiteral("on-battery")});
        ctx.nvidiaModuleSigned = false;
        ctx.secureBootEnabled = false;
        QCOMPARE(codes(checkMode(hw, Mode::Hybrid, Mode::Dedicated, ctx)), QStringList{QStringLiteral("on-battery")});

        // not NVIDIA: no Secure Boot warning
        const Hardware amd = detect("hybrid-amd");
        ctx.secureBootEnabled = true;
        ctx.onBattery = false;
        QVERIFY(checkMode(amd, Mode::Hybrid, Mode::Dedicated, ctx).isEmpty());
    }

    void readContextFromFixture()
    {
        const Root root(QString::fromUtf8(AGINO_GPU_FIXTURES) + QStringLiteral("/hybrid-nvidia"));
        const CheckContext ctx = readCheckContext(root);
        QVERIFY(!ctx.onBattery); // AC online
        QVERIFY(!ctx.secureBootEnabled);
        QVERIFY(ctx.nvidiaModuleSigned);
    }

    void readContextBatteryAndSecureBoot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const Root root(dir.path());
        // no supplies at all: not on battery
        QVERIFY(!readCheckContext(root).onBattery);

        const QString sys = QStringLiteral("/sys/class/power_supply");
        QVERIFY(root.writeFile(sys + QStringLiteral("/AC/type"), "Mains\n"));
        QVERIFY(root.writeFile(sys + QStringLiteral("/AC/online"), "0\n"));
        QVERIFY(root.writeFile(sys + QStringLiteral("/BAT0/type"), "Battery\n"));
        QVERIFY(readCheckContext(root).onBattery);
        QVERIFY(root.writeFile(sys + QStringLiteral("/AC/online"), "1\n"));
        QVERIFY(!readCheckContext(root).onBattery);
        // a USB-C charger counts as well
        QVERIFY(root.writeFile(sys + QStringLiteral("/AC/online"), "0\n"));
        QVERIFY(root.writeFile(sys + QStringLiteral("/ucsi/type"), "USB\n"));
        QVERIFY(root.writeFile(sys + QStringLiteral("/ucsi/online"), "1\n"));
        QVERIFY(!readCheckContext(root).onBattery);

        const QString var = QStringLiteral("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c");
        QVERIFY(root.writeFile(var, QByteArray("\x06\x00\x00\x00\x01", 5)));
        QVERIFY(readCheckContext(root).secureBootEnabled);
        QVERIFY(root.writeFile(var, QByteArray("\x06\x00\x00\x00\x00", 5)));
        QVERIFY(!readCheckContext(root).secureBootEnabled);

        QVERIFY(readCheckContext(root).nvidiaModuleSigned); // module not loaded
        QVERIFY(root.writeFile(QStringLiteral("/sys/module/nvidia/taint"), "POE\n"));
        QVERIFY(!readCheckContext(root).nvidiaModuleSigned);
        QVERIFY(root.writeFile(QStringLiteral("/sys/module/nvidia/taint"), "PO\n"));
        QVERIFY(readCheckContext(root).nvidiaModuleSigned);
    }
};

QTEST_APPLESS_MAIN(TstMode)
#include "tst_mode.moc"
