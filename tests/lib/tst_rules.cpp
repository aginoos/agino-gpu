// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/rules.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace AginoGpu;

class TstRules : public QObject
{
    Q_OBJECT

private:
    static bool touch(const QString &path, bool executable)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        f.write("#!/bin/sh\n");
        f.close();
        return f.setPermissions(executable ? QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                 | QFileDevice::ExeOwner
                                           : QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

private Q_SLOTS:
    void jsonRoundTrip()
    {
        RuleSet rules;
        rules.apps = {{QStringLiteral("steam")}, {QStringLiteral("blender")}};
        rules.paths = {{QStringLiteral("/x"), QStringLiteral("agino-gpu-x.desktop")},
                       {QStringLiteral("/y z"), QString()}};
        rules.pauseOnBattery = false;
        const auto parsed = ruleSetFromJson(ruleSetToJson(rules));
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->apps.size(), 2);
        QCOMPARE(parsed->apps.at(1).appId, QStringLiteral("blender"));
        QCOMPARE(parsed->paths.size(), 2);
        QCOMPARE(parsed->paths.at(0).launcherEntry, QStringLiteral("agino-gpu-x.desktop"));
        QCOMPARE(parsed->paths.at(1).path, QStringLiteral("/y z"));
        QVERIFY(parsed->paths.at(1).launcherEntry.isEmpty());
        QVERIFY(!parsed->pauseOnBattery);
    }

    void jsonExactBytes()
    {
        RuleSet rules;
        rules.apps = {{QStringLiteral("steam")}};
        rules.paths = {{QStringLiteral("/x"), QStringLiteral("agino-gpu-x.desktop")}};
        // QJsonObject sorts keys, so the output order is alphabetical
        QCOMPARE(ruleSetToJson(rules),
                 QByteArray("{\"apps\":[{\"appId\":\"steam\"}],"
                            "\"paths\":[{\"launcherEntry\":\"agino-gpu-x.desktop\",\"path\":\"/x\"}],"
                            "\"pauseOnBattery\":true,\"version\":1}"));
        QCOMPARE(ruleSetToJson(RuleSet()),
                 QByteArray("{\"apps\":[],\"paths\":[],\"pauseOnBattery\":true,\"version\":1}"));
    }

    void jsonDefaultsAndUnknownFields()
    {
        auto parsed = ruleSetFromJson(QByteArray("{}"));
        QVERIFY(parsed.has_value());
        QVERIFY(parsed->apps.isEmpty());
        QVERIFY(parsed->paths.isEmpty());
        QVERIFY(parsed->pauseOnBattery);

        parsed = ruleSetFromJson(QByteArray(
            "{\"version\":1,\"future\":[1,2],\"apps\":[{\"appId\":\"a\",\"gpu\":\"discrete\"}],"
            "\"paths\":[{\"path\":\"/p\"}]}"));
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->apps.size(), 1);
        QCOMPARE(parsed->paths.size(), 1);
        QVERIFY(parsed->paths.at(0).launcherEntry.isEmpty());
        QVERIFY(parsed->pauseOnBattery);
    }

    void jsonMalformed_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("garbage") << QByteArray("not json");
        QTest::newRow("truncated") << QByteArray("{\"apps\":[");
        QTest::newRow("array root") << QByteArray("[]");
        QTest::newRow("string root") << QByteArray("\"x\"");
        QTest::newRow("apps not array") << QByteArray("{\"apps\":{}}");
        QTest::newRow("app not object") << QByteArray("{\"apps\":[\"steam\"]}");
        QTest::newRow("appId missing") << QByteArray("{\"apps\":[{}]}");
        QTest::newRow("appId number") << QByteArray("{\"apps\":[{\"appId\":1}]}");
        QTest::newRow("paths not array") << QByteArray("{\"paths\":\"/x\"}");
        QTest::newRow("path missing") << QByteArray("{\"paths\":[{\"launcherEntry\":\"a\"}]}");
        QTest::newRow("path number") << QByteArray("{\"paths\":[{\"path\":3}]}");
        QTest::newRow("launcherEntry number") << QByteArray("{\"paths\":[{\"path\":\"/x\",\"launcherEntry\":1}]}");
        QTest::newRow("pause string") << QByteArray("{\"pauseOnBattery\":\"yes\"}");
        QTest::newRow("version string") << QByteArray("{\"version\":\"1\"}");
        QTest::newRow("version 2") << QByteArray("{\"version\":2}");
    }
    void jsonMalformed()
    {
        QFETCH(QByteArray, json);
        QVERIFY(!ruleSetFromJson(json).has_value());
    }

    void launcherName()
    {
        QCOMPARE(launcherEntryName(QStringLiteral("/usr/bin/foo")),
                 QStringLiteral("agino-gpu-761b2662e36a37ea.desktop"));
        QVERIFY(launcherEntryName(QStringLiteral("/usr/bin/foo"))
                != launcherEntryName(QStringLiteral("/usr/bin/bar")));
    }

    void launcherContentsPlain()
    {
        QCOMPARE(launcherEntryContents(QStringLiteral("/opt/game/run"), QStringLiteral("Game")),
                 QByteArray("[Desktop Entry]\n"
                            "Type=Application\n"
                            "Name=Game\n"
                            "Exec=/opt/game/run\n"
                            "PrefersNonDefaultGPU=true\n"
                            "X-Agino-Gpu-Rule=true\n"
                            "NoDisplay=false\n"));
    }

    void launcherContentsQuoting_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QByteArray>("execLine");
        QTest::newRow("space") << QStringLiteral("/opt/my game/run") << QByteArray("Exec=\"/opt/my game/run\"\n");
        QTest::newRow("percent") << QStringLiteral("/opt/100%/run") << QByteArray("Exec=/opt/100%%/run\n");
        QTest::newRow("quote") << QStringLiteral("/a\"b") << QByteArray("Exec=\"/a\\\\\"b\"\n");
        QTest::newRow("dollar") << QStringLiteral("/a$b") << QByteArray("Exec=\"/a\\\\$b\"\n");
        QTest::newRow("backtick") << QStringLiteral("/a`b") << QByteArray("Exec=\"/a\\\\`b\"\n");
        QTest::newRow("backslash") << QStringLiteral("/a\\b") << QByteArray("Exec=\"/a\\\\\\\\b\"\n");
        QTest::newRow("paren") << QStringLiteral("/a(1)/b") << QByteArray("Exec=\"/a(1)/b\"\n");
        QTest::newRow("tab") << QStringLiteral("/a\tb") << QByteArray("Exec=\"/a\\tb\"\n");
    }
    void launcherContentsQuoting()
    {
        QFETCH(QString, path);
        QFETCH(QByteArray, execLine);
        const QByteArray out = launcherEntryContents(path, QStringLiteral("N"));
        QVERIFY2(out.contains("\n" + execLine), out.constData());
    }

    void launcherContentsNameEscape()
    {
        const QByteArray out = launcherEntryContents(QStringLiteral("/x"), QStringLiteral("a\\b\nc"));
        QVERIFY(out.contains("\nName=a\\\\b\\nc\n"));
    }

    void mergeIntoEmpty()
    {
        const QStringList env{QStringLiteral("A=1"), QStringLiteral("B=2")};
        QCOMPARE(mergeRuleEnvironment({}, env),
                 (QStringList{QStringLiteral("A=1"), QStringLiteral("B=2"), QStringLiteral("AGINO_GPU_RULE=1")}));
    }

    void mergeKeepsForeignFirst()
    {
        const QStringList env{QStringLiteral("A=1")};
        const QStringList existing{QStringLiteral("X=1"), QStringLiteral("Y=a=b")};
        QCOMPARE(mergeRuleEnvironment(existing, env),
                 (QStringList{QStringLiteral("X=1"), QStringLiteral("Y=a=b"), QStringLiteral("A=1"),
                              QStringLiteral("AGINO_GPU_RULE=1")}));
    }

    void mergeForeignSameKeyWins()
    {
        const QStringList env{QStringLiteral("A=1"), QStringLiteral("B=2")};
        QCOMPARE(mergeRuleEnvironment({QStringLiteral("A=user=x")}, env),
                 (QStringList{QStringLiteral("A=user=x"), QStringLiteral("B=2"),
                              QStringLiteral("AGINO_GPU_RULE=1")}));
    }

    void mergeIsIdempotent()
    {
        const QStringList env{QStringLiteral("A=1"), QStringLiteral("B=2")};
        const QStringList once = mergeRuleEnvironment({QStringLiteral("X=1"), QStringLiteral("A=user")}, env);
        QCOMPARE(mergeRuleEnvironment(once, env), once);
        const QStringList plain = mergeRuleEnvironment({QStringLiteral("X=1")}, env);
        QCOMPARE(mergeRuleEnvironment(plain, env), plain);
    }

    void mergeReplacesPreviousEntries()
    {
        const QStringList oldEnv{QStringLiteral("A=1")};
        const QStringList newEnv{QStringLiteral("A=1"), QStringLiteral("B=2")};
        const QStringList merged = mergeRuleEnvironment(mergeRuleEnvironment({QStringLiteral("X=1")}, oldEnv), newEnv);
        QCOMPARE(merged,
                 (QStringList{QStringLiteral("X=1"), QStringLiteral("A=1"), QStringLiteral("B=2"),
                              QStringLiteral("AGINO_GPU_RULE=1")}));
    }

    void removeWithMarker()
    {
        const QStringList env{QStringLiteral("A=1"), QStringLiteral("B=2")};
        const QStringList existing{QStringLiteral("X=1"), QStringLiteral("A=user"), QStringLiteral("A=1"),
                                   QStringLiteral("B=2"), QStringLiteral("AGINO_GPU_RULE=1")};
        QCOMPARE(removeRuleEnvironment(existing, env),
                 (QStringList{QStringLiteral("X=1"), QStringLiteral("A=user")}));
    }

    void removeWithoutMarkerIsUnchanged()
    {
        const QStringList env{QStringLiteral("A=1")};
        const QStringList existing{QStringLiteral("A=1"), QStringLiteral("X=1")};
        QCOMPARE(removeRuleEnvironment(existing, env), existing);
        QVERIFY(removeRuleEnvironment({}, env).isEmpty());
    }

    void mergeThenRemoveRestoresForeign()
    {
        const QStringList env{QStringLiteral("A=1"), QStringLiteral("B=2")};
        const QStringList foreign{QStringLiteral("X=1"), QStringLiteral("B=mine")};
        QCOMPARE(removeRuleEnvironment(mergeRuleEnvironment(foreign, env), env), foreign);
    }

    void resolveExec()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString base = QFileInfo(dir.path()).canonicalFilePath();
        QVERIFY(touch(base + QStringLiteral("/bin/tool"), true));
        QVERIFY(touch(base + QStringLiteral("/bin/noexec"), false));
        QVERIFY(touch(base + QStringLiteral("/other/second"), true));
        QVERIFY(touch(base + QStringLiteral("/real/app one"), true));
        QVERIFY(QFile::link(base + QStringLiteral("/bin/tool"), base + QStringLiteral("/other/link")));
        QVERIFY(QFile::link(base + QStringLiteral("/real/app one"), base + QStringLiteral("/bin/applink")));
        const QString search = base + QStringLiteral("/missing:") + base + QStringLiteral("/bin:") + base
            + QStringLiteral("/other");
        const QString tool = base + QStringLiteral("/bin/tool");

        // bare names: first executable hit in the search path
        QCOMPARE(resolveExecProgram(QStringLiteral("tool"), search), tool);
        QCOMPARE(resolveExecProgram(QStringLiteral("second"), search), base + QStringLiteral("/other/second"));
        QCOMPARE(resolveExecProgram(QStringLiteral("noexec"), search), QString());
        QCOMPARE(resolveExecProgram(QStringLiteral("nothing"), search), QString());
        // symlinks are resolved
        QCOMPARE(resolveExecProgram(QStringLiteral("link"), search), tool);
        QCOMPARE(resolveExecProgram(QStringLiteral("applink"), search), base + QStringLiteral("/real/app one"));
        // absolute paths
        QCOMPARE(resolveExecProgram(tool, QString()), tool);
        QCOMPARE(resolveExecProgram(base + QStringLiteral("/bin/none"), search), QString());
        QCOMPARE(resolveExecProgram(QStringLiteral("./tool"), search), QString());
        // field codes
        QCOMPARE(resolveExecProgram(tool + QStringLiteral(" %U"), search), tool);
        QCOMPARE(resolveExecProgram(QStringLiteral("%f ") + tool, search), tool);
        QCOMPARE(resolveExecProgram(QStringLiteral("tool --flag %F %i %c %k"), search), tool);
        // quoting
        QCOMPARE(resolveExecProgram(QLatin1Char('"') + base + QStringLiteral("/real/app one\" %u"), search),
                 base + QStringLiteral("/real/app one"));
        QCOMPARE(resolveExecProgram(QStringLiteral("\"tool\" x"), search), tool);
        // env prefix
        QCOMPARE(resolveExecProgram(QStringLiteral("env FOO=1 BAR=\"a b\" tool %U"), search), tool);
        QCOMPARE(resolveExecProgram(QStringLiteral("/usr/bin/env A=b ") + tool, search), tool);
        QCOMPARE(resolveExecProgram(QStringLiteral("env A=b"), search), QString());
        // empty
        QCOMPARE(resolveExecProgram(QString(), search), QString());
        QCOMPARE(resolveExecProgram(QStringLiteral("%U"), search), QString());
    }
};

QTEST_APPLESS_MAIN(TstRules)
#include "tst_rules.moc"
