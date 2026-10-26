// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/chromium.h"

#include <QtTest>

using namespace AginoGpu;

class TstChromium : public QObject
{
    Q_OBJECT

private:
    Root root{QStringLiteral(AGINO_GPU_FIXTURES "/chromium")};
    const QStringList nvidiaEnv{QStringLiteral("__NV_PRIME_RENDER_OFFLOAD=1"),
                                QStringLiteral("__GLX_VENDOR_LIBRARY_NAME=nvidia")};

private Q_SLOTS:
    void nameMatch()
    {
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/lib/chromium/chromium")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/opt/google/chrome/google-chrome-stable")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/bin/microsoft-edge-dev")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/bin/vivaldi")));
    }

    void electronVersioned()
    {
        QVERIFY(isChromiumLike(root, QStringLiteral("/opt/versioned/electron37")));
        QVERIFY(!isChromiumLike(root, QStringLiteral("/opt/versioned/electronx")));
        QVERIFY(!isChromiumLike(root, QStringLiteral("/opt/versioned/electron-tools")));
    }

    void electronByAsar()
    {
        QVERIFY(isChromiumLike(root, QStringLiteral("/opt/fakeapp/fakeapp")));
    }

    void electronByParentResources()
    {
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/share/code/bin/code")));
    }

    void notChromium()
    {
        QVERIFY(!isChromiumLike(root, QStringLiteral("/usr/bin/plainprog")));
    }

    void noNvidiaEnvironment()
    {
        const QStringList args{QStringLiteral("--foo")};
        const QString prog = QStringLiteral("/usr/lib/chromium/chromium");
        QCOMPARE(withChromiumOffloadArguments(root, prog, args, {}), args);
        QCOMPARE(withChromiumOffloadArguments(root, prog, args, {QStringLiteral("DRI_PRIME=pci-0000_03_00_0")}),
                 args);
    }

    void existingAngleSwitch()
    {
        const QStringList args{QStringLiteral("--use-angle=gl"), QStringLiteral("x")};
        QCOMPARE(withChromiumOffloadArguments(root, QStringLiteral("/usr/lib/chromium/chromium"), args,
                                              nvidiaEnv),
                 args);
    }

    void nonChromiumUnchanged()
    {
        const QStringList args{QStringLiteral("a")};
        QCOMPARE(withChromiumOffloadArguments(root, QStringLiteral("/usr/bin/plainprog"), args, nvidiaEnv),
                 args);
    }

    void insertionOrder()
    {
        const QStringList args{QStringLiteral("--foo"), QStringLiteral("url")};
        const QStringList expected{
            QStringLiteral("--use-angle=vulkan"),
            QStringLiteral("--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"),
            QStringLiteral("--foo"),
            QStringLiteral("url"),
        };
        QCOMPARE(withChromiumOffloadArguments(root, QStringLiteral("/usr/lib/chromium/chromium"), args,
                                              nvidiaEnv),
                 expected);
        QCOMPARE(chromiumOffloadArguments(), expected.mid(0, 2));
    }
};

QTEST_APPLESS_MAIN(TstChromium)
#include "tst_chromium.moc"
