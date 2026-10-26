// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/detect.h"
#include "agino-gpu/dgpuusers.h"

#include <QtTest>

using namespace AginoGpu;

namespace {

Root fixtureRoot()
{
    return Root(QString::fromUtf8(AGINO_GPU_FIXTURES) + QStringLiteral("/proc-users"));
}

QList<quint32> pids(const QList<GpuUser> &users)
{
    QList<quint32> out;
    for (const GpuUser &u : users)
        out.append(u.pid);
    return out;
}

} // namespace

class TstDgpuUsers : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nvidiaUsers()
    {
        const Root root = fixtureRoot();
        const Hardware hw = detectHardware(root);
        QVERIFY(hw.discreteGpu());
        const Gpu &dgpu = *hw.discreteGpu();
        QCOMPARE(dgpu.vendor, Vendor::Nvidia);

        const QList<GpuUser> users = findGpuUsers(root, dgpu);
        // 101: renderD129 (the dGPU's render node), 102: /dev/nvidia*, 103: card1 + nvidia-modeset,
        // 106: /dev/nvidia0
        QCOMPARE(pids(users), (QList<quint32>{101, 102, 103, 106}));
        QCOMPARE(users.at(0).name, QStringLiteral("firefox"));
        QCOMPARE(users.at(0).uid, quint32(1000));
        QCOMPARE(users.at(1).name, QStringLiteral("python3"));
        QCOMPARE(users.at(2).name, QStringLiteral("Xorg"));
        QCOMPARE(users.at(2).uid, quint32(0));
        // comm is cut to 15 characters; the executable's file name gives the full one
        QCOMPARE(users.at(3).name, QStringLiteral("dde-control-center"));
    }

    void onlyUidFilter()
    {
        const Root root = fixtureRoot();
        const Gpu &dgpu = *detectHardware(root).discreteGpu();
        QCOMPARE(pids(findGpuUsers(root, dgpu, quint32(1000))), (QList<quint32>{101, 102, 106}));
        QCOMPARE(pids(findGpuUsers(root, dgpu, quint32(0))), QList<quint32>{103});
        QVERIFY(findGpuUsers(root, dgpu, quint32(4242)).isEmpty());
        // 104 only has the integrated card open
        QVERIFY(findGpuUsers(root, dgpu, quint32(1001)).isEmpty());
    }

    void integratedGpuNodesOnly()
    {
        const Root root = fixtureRoot();
        const Hardware hw = detectHardware(root);
        // The integrated Intel GPU: only its own card/render nodes count, never /dev/nvidia*.
        const Gpu &igpu = *hw.integrated();
        QCOMPARE(igpu.vendor, Vendor::Intel);
        QCOMPARE(pids(findGpuUsers(root, igpu)), (QList<quint32>{104, 105}));
    }

    void nvidiaNodesIgnoredForOtherVendors()
    {
        const Root root = fixtureRoot();
        Gpu fake = *detectHardware(root).discreteGpu();
        fake.vendor = Vendor::Amd; // same PCI address, but /dev/nvidia* must not count
        QCOMPARE(pids(findGpuUsers(root, fake)), (QList<quint32>{101, 103}));
    }

    void unknownGpuHasNoUsers()
    {
        Gpu nowhere;
        nowhere.pciAddress = QStringLiteral("0000:99:00.0");
        nowhere.vendor = Vendor::Amd;
        QVERIFY(findGpuUsers(fixtureRoot(), nowhere).isEmpty());
    }

    void missingProcIsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Gpu gpu;
        gpu.vendor = Vendor::Nvidia;
        gpu.pciAddress = QStringLiteral("0000:01:00.0");
        QVERIFY(findGpuUsers(Root(dir.path()), gpu).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstDgpuUsers)
#include "tst_dgpuusers.moc"
