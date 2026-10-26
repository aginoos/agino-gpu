// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/detect.h"

#include <QProcess>
#include <QtTest>

using namespace AginoGpu;

namespace {

Root fixture(const char *name)
{
    return Root(QString::fromUtf8(AGINO_GPU_FIXTURES) + QLatin1Char('/') + QString::fromUtf8(name));
}

} // namespace

class TstDetect : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void classes_data()
    {
        QTest::addColumn<QString>("fixture");
        QTest::addColumn<int>("hwClass");
        QTest::addColumn<int>("gpuCount");
        QTest::addColumn<QString>("classString");
        QTest::newRow("hybrid-nvidia") << "hybrid-nvidia" << int(HardwareClass::Hybrid) << 2 << "hybrid";
        QTest::newRow("hybrid-nvidia-mux")
            << "hybrid-nvidia-mux" << int(HardwareClass::HybridMux) << 2 << "hybrid-mux";
        QTest::newRow("mux-dedicated-active")
            << "mux-dedicated-active" << int(HardwareClass::HybridMux) << 1 << "hybrid-mux";
        QTest::newRow("hybrid-amd") << "hybrid-amd" << int(HardwareClass::Hybrid) << 2 << "hybrid";
        QTest::newRow("desktop-dgpu-display")
            << "desktop-dgpu-display" << int(HardwareClass::Desktop) << 2 << "desktop";
        QTest::newRow("desktop-igpu-display")
            << "desktop-igpu-display" << int(HardwareClass::Desktop) << 2 << "desktop";
        QTest::newRow("egpu") << "egpu" << int(HardwareClass::Egpu) << 2 << "egpu";
        QTest::newRow("single") << "single" << int(HardwareClass::Single) << 1 << "single";
    }
    void classes()
    {
        QFETCH(QString, fixture);
        QFETCH(int, hwClass);
        QFETCH(int, gpuCount);
        QFETCH(QString, classString);
        const Hardware hw = detectHardware(Root(QString::fromUtf8(AGINO_GPU_FIXTURES) + QLatin1Char('/') + fixture));
        QCOMPARE(int(hw.hardwareClass), hwClass);
        QCOMPARE(hw.gpus.size(), gpuCount);
        QCOMPARE(hardwareClassToString(hw.hardwareClass), classString);
    }

    void hybridNvidiaDetails()
    {
        const Hardware hw = detectHardware(fixture("hybrid-nvidia"));
        QCOMPARE(hw.integratedIndex, 0);
        QCOMPARE(hw.discreteIndex, 1);
        QVERIFY(hw.hasInternalPanel);
        QCOMPARE(hw.internalPanelGpu, QStringLiteral("0000:00:02.0"));
        QVERIFY(!hw.mux.available);
        QVERIFY(!hw.asusDgpuDisable);

        const Gpu &igpu = *hw.integrated();
        QCOMPARE(igpu.pciAddress, QStringLiteral("0000:00:02.0"));
        QCOMPARE(igpu.vendor, Vendor::Intel);
        QCOMPARE(igpu.vendorId, quint16(0x8086));
        QCOMPARE(igpu.driver, QStringLiteral("i915"));
        QVERIFY(igpu.bootVga);
        QVERIFY(!igpu.discrete);
        QVERIFY(!igpu.removable);
        QCOMPARE(igpu.drmCard, QStringLiteral("card0"));
        QCOMPARE(igpu.connectedOutputs, QStringList{QStringLiteral("eDP-1")});
        QVERIFY(igpu.upstreamPort.isEmpty());
        QCOMPARE(igpu.name, QStringLiteral("Intel Graphics"));

        const Gpu &dgpu = *hw.discreteGpu();
        QCOMPARE(dgpu.pciAddress, QStringLiteral("0000:01:00.0"));
        QCOMPARE(dgpu.vendor, Vendor::Nvidia);
        QCOMPARE(dgpu.vendorId, quint16(0x10de));
        QCOMPARE(dgpu.deviceId, quint16(0x2c19));
        QCOMPARE(dgpu.driver, QStringLiteral("nvidia"));
        QVERIFY(!dgpu.bootVga);
        QVERIFY(dgpu.discrete);
        QVERIFY(!dgpu.removable);
        QCOMPARE(dgpu.drmCard, QStringLiteral("card1"));
        QVERIFY(dgpu.connectedOutputs.isEmpty());
        QCOMPARE(dgpu.allOutputs, (QStringList{QStringLiteral("DP-1"), QStringLiteral("HDMI-A-1")}));
        QVERIFY(dgpu.runtimePmSupported);
        QCOMPARE(dgpu.runtimeStatus, QStringLiteral("suspended"));
        QCOMPARE(dgpu.functions, QStringList{QStringLiteral("0000:01:00.1")});
        QCOMPARE(dgpu.upstreamPort, QStringLiteral("0000:00:01.0"));
        QCOMPARE(dgpu.name, QStringLiteral("NVIDIA GeForce RTX 5080 Max-Q / Mobile"));
    }

    void muxStates()
    {
        const Hardware hybrid = detectHardware(fixture("hybrid-nvidia-mux"));
        QVERIFY(hybrid.mux.available);
        QVERIFY(!hybrid.mux.dedicatedActive);
        QCOMPARE(hybrid.mux.sysfsPath, QStringLiteral("/sys/devices/platform/asus-nb-wmi/gpu_mux_mode"));
        QVERIFY(hybrid.asusDgpuDisable);
        QCOMPARE(hybrid.integratedIndex, 0);
        QCOMPARE(hybrid.discreteIndex, 1);

        const Hardware dedicated = detectHardware(fixture("mux-dedicated-active"));
        QVERIFY(dedicated.mux.available);
        QVERIFY(dedicated.mux.dedicatedActive);
        QCOMPARE(dedicated.integratedIndex, -1);
        QCOMPARE(dedicated.discreteIndex, 0);
        QVERIFY(dedicated.integrated() == nullptr);
        QVERIFY(dedicated.hasInternalPanel);
        QCOMPARE(dedicated.internalPanelGpu, QStringLiteral("0000:01:00.0"));
        QVERIFY(dedicated.gpus.at(0).discrete);
        QCOMPARE(dedicated.gpus.at(0).connectedOutputs, QStringList{QStringLiteral("eDP-1")});
    }

    void amd()
    {
        const Hardware hw = detectHardware(fixture("hybrid-amd"));
        QCOMPARE(hw.gpus.size(), 2);
        // sorted by PCI address: 0000:03:00.0 (dGPU) first, 0000:c5:00.0 (iGPU) second
        QCOMPARE(hw.discreteIndex, 0);
        QCOMPARE(hw.integratedIndex, 1);
        QCOMPARE(hw.discreteGpu()->pciAddress, QStringLiteral("0000:03:00.0"));
        QCOMPARE(hw.discreteGpu()->vendor, Vendor::Amd);
        QCOMPARE(hw.discreteGpu()->driver, QStringLiteral("amdgpu"));
        QCOMPARE(hw.discreteGpu()->drmCard, QStringLiteral("card0"));
        QCOMPARE(hw.integrated()->drmCard, QStringLiteral("card1"));
        QCOMPARE(hw.integrated()->upstreamPort, QStringLiteral("0000:c0:08.1"));
        QVERIFY(hw.integrated()->bootVga);
        QVERIFY(hw.discreteGpu()->name.isEmpty()); // fixture has no pci.ids
    }

    void desktops()
    {
        const Hardware onDgpu = detectHardware(fixture("desktop-dgpu-display"));
        QVERIFY(!onDgpu.hasInternalPanel);
        QVERIFY(onDgpu.internalPanelGpu.isEmpty());
        QCOMPARE(onDgpu.discreteGpu()->connectedOutputs, QStringList{QStringLiteral("HDMI-A-1")});
        QVERIFY(onDgpu.integrated()->connectedOutputs.isEmpty());

        const Hardware onIgpu = detectHardware(fixture("desktop-igpu-display"));
        QCOMPARE(onIgpu.integrated()->connectedOutputs, QStringList{QStringLiteral("DP-1")});
        QVERIFY(onIgpu.discreteGpu()->connectedOutputs.isEmpty());
    }

    void egpuIsRemovable()
    {
        const Hardware hw = detectHardware(fixture("egpu"));
        QVERIFY(hw.discreteGpu());
        QVERIFY(hw.discreteGpu()->removable);
        QVERIFY(hw.discreteGpu()->discrete);
        QCOMPARE(hw.discreteGpu()->upstreamPort, QStringLiteral("0000:04:01.0"));
        QVERIFY(!hw.integrated()->removable);
        QVERIFY(hw.hasInternalPanel);
    }

    void singleHasNoDiscrete()
    {
        const Hardware hw = detectHardware(fixture("single"));
        QCOMPARE(hw.discreteIndex, -1);
        QCOMPARE(hw.integratedIndex, -1);
        QVERIFY(!hw.gpus.at(0).discrete);
    }

    void emptyRoot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const Hardware hw = detectHardware(Root(dir.path()));
        QVERIFY(hw.gpus.isEmpty());
        QCOMPARE(hw.hardwareClass, HardwareClass::Single);
    }

    void audioFunctionIsNotAGpu()
    {
        const Hardware hw = detectHardware(fixture("hybrid-nvidia"));
        for (const Gpu &g : hw.gpus)
            QVERIFY(g.pciAddress != QLatin1String("0000:01:00.1"));
    }

    void refreshReadsVolatileFields()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString copy = dir.path() + QStringLiteral("/hybrid-nvidia");
        QVERIFY(QProcess::execute(QStringLiteral("cp"), {QStringLiteral("-a"), QString::fromUtf8(AGINO_GPU_FIXTURES "/hybrid-nvidia"), copy}) == 0);
        const Root root(copy);
        Hardware hw = detectHardware(root);
        Gpu gpu = *hw.discreteGpu();
        QCOMPARE(gpu.runtimeStatus, QStringLiteral("suspended"));
        QVERIFY(gpu.connectedOutputs.isEmpty());

        QVERIFY(root.writeFile(QStringLiteral("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/power/runtime_status"),
                               "active\n"));
        QVERIFY(root.writeFile(QStringLiteral("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/drm/card1/card1-DP-1/status"),
                               "connected\n"));
        refreshGpu(root, gpu);
        QCOMPARE(gpu.runtimeStatus, QStringLiteral("active"));
        QCOMPARE(gpu.connectedOutputs, QStringList{QStringLiteral("DP-1")});
        QCOMPARE(gpu.pciAddress, QStringLiteral("0000:01:00.0")); // untouched
    }

    void names()
    {
        QCOMPARE(vendorName(Vendor::Intel), QStringLiteral("Intel"));
        QCOMPARE(vendorName(Vendor::Amd), QStringLiteral("AMD"));
        QCOMPARE(vendorName(Vendor::Nvidia), QStringLiteral("NVIDIA"));
        QCOMPARE(vendorName(Vendor::Unknown), QStringLiteral("Unknown"));
        QCOMPARE(hardwareClassToString(HardwareClass::Egpu), QStringLiteral("egpu"));
    }
};

QTEST_APPLESS_MAIN(TstDetect)
#include "tst_detect.moc"
