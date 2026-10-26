// SPDX-License-Identifier: GPL-3.0-or-later
#include "agino-gpu/rules.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace AginoGpu {

namespace {

// Exec= quoting (Desktop Entry spec, "The Exec key"). An argument that contains one of the
// reserved characters  space tab newline " ' \ > < ~ | & ; $ * ? # ( ) `  must be enclosed in
// double quotes; inside the quotes the characters  " ` $ \  are escaped with a backslash. A
// literal % is written %%. On top of that, the Exec value is itself a desktop-file "string"
// value, which is unescaped once when the file is read (\\ -> \, \n -> newline, \t -> tab), so
// every backslash of the Exec-level text is doubled again (the quoting backslash of \" becomes
// \\" in the file), and newline/tab are written as \n / \t.
QString quoteExecArgument(const QString &arg)
{
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    QString inner;
    bool quote = false;
    for (const QChar c : arg) {
        if (reserved.contains(c))
            quote = true;
        if (c == QLatin1Char('%'))
            inner += QStringLiteral("%%");
        else
            inner += c;
    }
    if (!quote)
        return inner;
    QString escaped;
    for (const QChar c : std::as_const(inner)) {
        if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$') || c == QLatin1Char('\\'))
            escaped += QLatin1Char('\\');
        escaped += c;
    }
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// Desktop-file string value escaping (backslash, newline, tab).
QString escapeDesktopString(const QString &s)
{
    QString out;
    for (const QChar c : s) {
        if (c == QLatin1Char('\\'))
            out += QStringLiteral("\\\\");
        else if (c == QLatin1Char('\n'))
            out += QStringLiteral("\\n");
        else if (c == QLatin1Char('\t'))
            out += QStringLiteral("\\t");
        else
            out += c;
    }
    return out;
}

QString keyOf(const QString &entry)
{
    const qsizetype eq = entry.indexOf(QLatin1Char('='));
    return eq < 0 ? entry : entry.left(eq);
}

// Splits an Exec value (already unescaped as a desktop-file string) into arguments: whitespace
// separates arguments, double quotes group, and inside quotes a backslash escapes the next
// character. Field codes are not interpreted here.
QStringList splitExec(const QString &exec)
{
    QStringList args;
    QString cur;
    bool inQuotes = false;
    bool have = false;
    for (qsizetype i = 0; i < exec.size(); ++i) {
        const QChar c = exec.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('\\') && i + 1 < exec.size()) {
                cur += exec.at(++i);
            } else if (c == QLatin1Char('"')) {
                inQuotes = false;
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
            have = true;
        } else if (c.isSpace()) {
            if (have) {
                args << cur;
                cur.clear();
                have = false;
            }
        } else {
            cur += c;
            have = true;
        }
    }
    if (have)
        args << cur;
    return args;
}

// Drops field codes (%f %F %u %U %i %c %k and the deprecated ones) and turns %% into %.
QString stripFieldCodes(const QString &arg)
{
    QString out;
    for (qsizetype i = 0; i < arg.size(); ++i) {
        if (arg.at(i) == QLatin1Char('%') && i + 1 < arg.size()) {
            const QChar next = arg.at(++i);
            if (next == QLatin1Char('%'))
                out += QLatin1Char('%');
            continue; // any other field code is dropped
        }
        out += arg.at(i);
    }
    return out;
}

bool isEnvAssignment(const QString &arg)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*="));
    return re.match(arg).hasMatch();
}

} // namespace

QByteArray ruleSetToJson(const RuleSet &rules)
{
    QJsonArray apps;
    for (const AppRule &a : rules.apps)
        apps.append(QJsonObject{{QStringLiteral("appId"), a.appId}});
    QJsonArray paths;
    for (const PathRule &p : rules.paths) {
        paths.append(QJsonObject{{QStringLiteral("path"), p.path},
                                 {QStringLiteral("launcherEntry"), p.launcherEntry}});
    }
    const QJsonObject root{{QStringLiteral("version"), 1},
                           {QStringLiteral("apps"), apps},
                           {QStringLiteral("paths"), paths},
                           {QStringLiteral("pauseOnBattery"), rules.pauseOnBattery}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

std::optional<RuleSet> ruleSetFromJson(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject root = doc.object();
    RuleSet rules;

    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isUndefined() && (!version.isDouble() || version.toInt() != 1))
        return std::nullopt;

    const QJsonValue apps = root.value(QStringLiteral("apps"));
    if (!apps.isUndefined()) {
        if (!apps.isArray())
            return std::nullopt;
        for (const QJsonValue &v : apps.toArray()) {
            const QJsonValue id = v.toObject().value(QStringLiteral("appId"));
            if (!v.isObject() || !id.isString())
                return std::nullopt;
            rules.apps.append({id.toString()});
        }
    }

    const QJsonValue paths = root.value(QStringLiteral("paths"));
    if (!paths.isUndefined()) {
        if (!paths.isArray())
            return std::nullopt;
        for (const QJsonValue &v : paths.toArray()) {
            const QJsonObject o = v.toObject();
            const QJsonValue path = o.value(QStringLiteral("path"));
            const QJsonValue entry = o.value(QStringLiteral("launcherEntry"));
            if (!v.isObject() || !path.isString()
                || (!entry.isUndefined() && !entry.isString())) {
                return std::nullopt;
            }
            rules.paths.append({path.toString(), entry.toString()});
        }
    }

    const QJsonValue pause = root.value(QStringLiteral("pauseOnBattery"));
    if (!pause.isUndefined()) {
        if (!pause.isBool())
            return std::nullopt;
        rules.pauseOnBattery = pause.toBool();
    }
    return rules;
}

QString launcherEntryName(const QString &canonicalPath)
{
    const QByteArray hash =
        QCryptographicHash::hash(canonicalPath.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("agino-gpu-%1.desktop").arg(QString::fromLatin1(hash.left(16)));
}

QByteArray launcherEntryContents(const QString &canonicalPath, const QString &name)
{
    const QString text = QStringLiteral("[Desktop Entry]\n"
                                        "Type=Application\n"
                                        "Name=%1\n"
                                        "Exec=%2\n"
                                        "PrefersNonDefaultGPU=true\n"
                                        "X-Agino-Gpu-Rule=true\n"
                                        "NoDisplay=false\n")
                             .arg(escapeDesktopString(name),
                                  escapeDesktopString(quoteExecArgument(canonicalPath)));
    return text.toUtf8();
}

QStringList mergeRuleEnvironment(const QStringList &existing, const QStringList &offloadEnv)
{
    const QString marker = QLatin1String(kRuleMarker);
    // Drop the entries of a previous merge first (exact matches only, and only when marked).
    QStringList result = removeRuleEnvironment(existing, offloadEnv);
    QStringList foreignKeys;
    for (const QString &e : std::as_const(result))
        foreignKeys << keyOf(e);
    for (const QString &e : offloadEnv) {
        if (!foreignKeys.contains(keyOf(e)))
            result << e;
    }
    result << marker;
    return result;
}

QStringList removeRuleEnvironment(const QStringList &existing, const QStringList &offloadEnv)
{
    const QString marker = QLatin1String(kRuleMarker);
    if (!existing.contains(marker))
        return existing;
    QStringList result;
    for (const QString &e : existing) {
        if (e != marker && !offloadEnv.contains(e))
            result << e;
    }
    return result;
}

// Uses the real file system on purpose (not Root): the DAM patch runs this on the user's real
// paths and the result must be what the kernel would execute.
QString resolveExecProgram(const QString &exec, const QString &searchPath)
{
    QStringList args;
    for (const QString &raw : splitExec(exec)) {
        const QString arg = stripFieldCodes(raw);
        if (!arg.isEmpty())
            args << arg;
    }

    qsizetype i = 0;
    if (i < args.size() && (args.at(i) == QLatin1String("env") || args.at(i) == QLatin1String("/usr/bin/env"))) {
        ++i;
        while (i < args.size() && isEnvAssignment(args.at(i)))
            ++i;
    }
    if (i >= args.size())
        return QString();

    const QString program = args.at(i);
    if (program.startsWith(QLatin1Char('/'))) {
        const QFileInfo info(program);
        return info.isFile() ? info.canonicalFilePath() : QString();
    }
    if (program.contains(QLatin1Char('/')))
        return QString();
    const QStringList dirs = searchPath.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &dir : dirs) {
        const QFileInfo info(QDir(dir).filePath(program));
        if (info.isFile() && info.isExecutable())
            return info.canonicalFilePath();
    }
    return QString();
}

} // namespace AginoGpu
