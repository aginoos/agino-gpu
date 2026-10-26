// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/chromium.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace AginoGpu;

namespace {
QStringList L(std::initializer_list<const char *> items)
{
    QStringList list;
    for (const char *item : items)
        list << QString::fromLatin1(item);
    return list;
}
} // namespace

class TstChromium : public QObject
{
    Q_OBJECT

private:
    Root root{QStringLiteral(AGINO_GPU_FIXTURES "/chromium")};

private Q_SLOTS:
    void runtimeFiles()
    {
        QVERIFY(hasChromiumRuntime(root, QStringLiteral("/usr/lib/chromium")));
        QVERIFY(hasChromiumRuntime(root, QStringLiteral("/opt/microsoft/msedge")));
        QVERIFY(!hasChromiumRuntime(root, QStringLiteral("/partial")));
        QVERIFY(!hasChromiumRuntime(root, QStringLiteral("/usr/bin")));
    }

    void detectedInProgramDirectory()
    {
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/lib/chromium/chromium")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/opt/google/chrome/chrome")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/opt/microsoft/msedge/msedge")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/appimage-root/obsidian")));
    }

    void detectedInParentDirectory()
    {
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/share/code/bin/code")));
        QVERIFY(isChromiumLike(root, QStringLiteral("/usr/share/code/code")));
    }

    void notDetected()
    {
        // Debian's wrapper script: its runtime is in /usr/lib/chromium, handled by --chromium.
        QVERIFY(!isChromiumLike(root, QStringLiteral("/usr/bin/chromium")));
        QVERIFY(!isChromiumLike(root, QStringLiteral("/usr/bin/plainprog")));
        QVERIFY(!isChromiumLike(root, QStringLiteral("/partial/app")));
    }

    void appImageHeader()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Root tmp(dir.path());
        const auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(bytes);
        };
        write(QStringLiteral("type2"), QByteArray("\x7f" "ELF\x02\x01\x01\x00" "AI\x02rest", 15));
        write(QStringLiteral("type1"), QByteArray("\x7f" "ELF\x02\x01\x01\x00" "AI\x01rest", 15));
        write(QStringLiteral("plainelf"), QByteArray("\x7f" "ELF\x02\x01\x01\x00\x00\x00\x00rest", 15));
        write(QStringLiteral("script"), QByteArray("#!/bin/sh\nexit 0\n"));
        write(QStringLiteral("short"), QByteArray("\x7f" "ELF"));
        QVERIFY(isAppImage(tmp, QStringLiteral("/type2")));
        QVERIFY(!isAppImage(tmp, QStringLiteral("/type1")));
        QVERIFY(!isAppImage(tmp, QStringLiteral("/plainelf")));
        QVERIFY(!isAppImage(tmp, QStringLiteral("/script")));
        QVERIFY(!isAppImage(tmp, QStringLiteral("/short")));
        QVERIFY(!isAppImage(tmp, QStringLiteral("/missing")));
    }

    void plainArguments()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--foo", "url"})),
                 L({"--use-angle=vulkan", "--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
                    "--foo", "url"}));
        QCOMPARE(withChromiumOffloadArguments({}),
                 L({"--use-angle=vulkan", "--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"}));
    }

    void mergesIntoEnableFeatures()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--enable-features=WebRTCPipeWireCapturer", "url"})),
                 L({"--use-angle=vulkan",
                    "--enable-features=WebRTCPipeWireCapturer,Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
                    "url"}));
    }

    void onlyLastEnableFeaturesRewritten()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--enable-features=A", "--x", "--enable-features=B"})),
                 L({"--use-angle=vulkan", "--enable-features=A", "--x",
                    "--enable-features=B,Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"}));
    }

    void noDuplicateFeatures()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--enable-features=Vulkan,A<Trial:p/1"})),
                 L({"--use-angle=vulkan",
                    "--enable-features=Vulkan,A<Trial:p/1,VulkanFromANGLE,DefaultANGLEVulkan"}));
        QCOMPARE(withChromiumOffloadArguments(L({"--enable-features=A, Vulkan"})),
                 L({"--use-angle=vulkan", "--enable-features=A, Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"}));
    }

    void disabledFeatureKeepsArguments()
    {
        const QStringList a = L({"--disable-features=Foo,Vulkan", "url"});
        QCOMPARE(withChromiumOffloadArguments(a), a);
        const QStringList b = L({"--disable-features=VulkanFromANGLE<Trial"});
        QCOMPARE(withChromiumOffloadArguments(b), b);
        const QStringList c = L({"--disable-features=DefaultANGLEVulkan:x/y", "--enable-features=A"});
        QCOMPARE(withChromiumOffloadArguments(c), c);
        // Chromium trims whitespace around the names of a feature list.
        const QStringList d = L({"--disable-features=Foo, Vulkan"});
        QCOMPARE(withChromiumOffloadArguments(d), d);
    }

    void onlyLastDisableFeaturesCounts()
    {
        // Chromium reads one --disable-features (the last); an earlier one with Vulkan is ignored.
        QCOMPARE(withChromiumOffloadArguments(L({"--disable-features=Vulkan", "--disable-features=Foo"})),
                 L({"--use-angle=vulkan", "--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
                    "--disable-features=Vulkan", "--disable-features=Foo"}));
    }

    void unrelatedDisableFeatures()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--disable-features=Foo"})),
                 L({"--use-angle=vulkan", "--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
                    "--disable-features=Foo"}));
    }

    void existingAngleSwitch()
    {
        const QStringList a = L({"--use-angle=gl", "x"});
        QCOMPARE(withChromiumOffloadArguments(a), a);
        const QStringList b = L({"-use-angle=gl"});
        QCOMPARE(withChromiumOffloadArguments(b), b);
    }

    void singleDashPrefix()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"-enable-features=X"})),
                 L({"--use-angle=vulkan", "-enable-features=X,Vulkan,VulkanFromANGLE,DefaultANGLEVulkan"}));
    }

    void terminatorStopsParsing()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--", "--enable-features=X", "--use-angle=gl"})),
                 L({"--use-angle=vulkan", "--enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
                    "--", "--enable-features=X", "--use-angle=gl"}));
    }

    void urlsAndFilesKept()
    {
        QCOMPARE(withChromiumOffloadArguments(L({"--enable-features=X", "https://example.org", "/tmp/a.html"})),
                 L({"--use-angle=vulkan", "--enable-features=X,Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
                    "https://example.org", "/tmp/a.html"}));
    }
};

QTEST_APPLESS_MAIN(TstChromium)
#include "tst_chromium.moc"
