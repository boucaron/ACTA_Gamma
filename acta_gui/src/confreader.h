#pragma once

// Qt reader for the per-machine config file (docs/plans/acta-config-file.md)
// — the GUI's counterpart of acta_conf_parse in acta_db/src/conf.c: one
// JSON object with at most "api_key" (string), "db" (string),
// "max_chars" / "timeout" (positive integer).  Fail-closed on
// not-an-object, unknown key, wrong type, or malformed JSON — the same
// rules as the C parser, so all three binaries reject the same bad file.
//
// A missing or unreadable file is NOT an error: the file is simply
// unavailable as a fallback (missing = true, valid = true).
//
// Permission gate (work item 5, mirror of acta_conf_read in
// acta_db/src/conf.c): the file may hold a secret ("api_key"), so it must
// be 0600 (owner read/write only).  Group- or other-readable -> fail-closed
// BEFORE the contents are read.  stat failure (no file) falls through to
// the missing-file path.  On Windows (MSYS2/MinGW) the mode bits are
// meaningless (always 0666), so the gate warns and reads the file anyway
// (best-effort, not enforced).
//
// Shared by the two GUI read sites (work items 3 and 6):
//   - MainWindow::defaultDbPath consumes "db" (work item 3);
//   - RunnerWorker::runInThread consumes "api_key" / "max_chars" /
//     "timeout" (work item 6), applying the shared key policy
//     acta_conf_api_key_status from acta_db/conf.h.

#include <cstdio>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <climits>

#include <sys/stat.h>
#include <sys/types.h>

struct ActaConfFile {
    QString db;          // "db" string; empty when absent
    QString apiKey;      // "api_key" string; empty when absent or ""
    bool apiKeyPresent = false;  // "api_key" key present in the file,
                                 // even with an empty value (the shared
                                 // key policy distinguishes NULL/absent
                                 // from ""/empty)
    long maxChars = 0;   // "max_chars" positive integer; 0 when absent
    long timeout = 0;    // "timeout" positive integer (seconds); 0 when absent
    bool valid = true;   // false -> contract/parse error
    bool missing = false;
    QString error;       // one-line diagnostic when !valid
};

inline bool confPosInt(const QJsonValue &v, long *out)
{
    if (!v.isDouble())
        return false;
    const double d = v.toDouble();
    if (d <= 0.0 || d > (double)LONG_MAX)
        return false;
    if (d != (double)(long)d)
        return false;
    *out = (long)d;
    return true;
}

inline ActaConfFile readActaConfFile(const QString &path)
{
    ActaConfFile c;

    {
        struct stat st;
        if (stat(path.toLocal8Bit().constData(), &st) == 0 &&
            (st.st_mode & (S_IRGRP | S_IROTH)) != 0) {
#ifdef _WIN32
            fprintf(stderr,
                    "warning: config file %s is group- or other-readable "
                    "per its mode bits; the mode check is best-effort on "
                    "Windows (MSYS2/MinGW always reports 0666), the file "
                    "will be read anyway\n",
                    path.toLocal8Bit().constData());
#else
            c.valid = false;
            c.error =
                QStringLiteral(
                    "config file %1 is group- or other-readable; it must "
                    "be 0600 (owner read/write only)")
                    .arg(path);
            return c;
#endif
        }
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        c.missing = true; // missing or unreadable: unavailable, not an error
        return c;
    }
    const QByteArray raw = f.readAll();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError) {
        c.valid = false;
        c.error = QStringLiteral("config is not valid JSON: %1")
                        .arg(pe.errorString());
        return c;
    }
    if (!doc.isObject()) {
        c.valid = false;
        c.error = QStringLiteral("config root is not an object");
        return c;
    }
    const QJsonObject obj = doc.object();

    // Unknown top-level key -> contract violation (mirror of the
    // known[] check in acta_db/conf.c).
    static const char *known[] = { "api_key", "db", "max_chars", "timeout" };
    for (const QString &k : obj.keys()) {
        bool ok = false;
        for (const char *kn : known) {
            if (k == QLatin1String(kn)) { ok = true; break; }
        }
        if (!ok) {
            c.valid = false;
            c.error = QStringLiteral("unknown config key: '%1'").arg(k);
            return c;
        }
    }

    if (obj.contains("api_key")) {
        if (!obj["api_key"].isString()) {
            c.valid = false;
            c.error = QStringLiteral("config key 'api_key' must be a string");
            return c;
        }
        c.apiKeyPresent = true;
        c.apiKey = obj["api_key"].toString();
    }
    if (obj.contains("db")) {
        if (!obj["db"].isString()) {
            c.valid = false;
            c.error = QStringLiteral("config key 'db' must be a string");
            return c;
        }
        c.db = obj["db"].toString();
    }
    long v = 0;
    if (obj.contains("max_chars") && !confPosInt(obj["max_chars"], &v)) {
        c.valid = false;
        c.error = QStringLiteral(
            "config key 'max_chars' must be a positive integer");
        return c;
    }
    c.maxChars = v;
    v = 0;
    if (obj.contains("timeout") && !confPosInt(obj["timeout"], &v)) {
        c.valid = false;
        c.error = QStringLiteral(
            "config key 'timeout' must be a positive integer");
        return c;
    }
    c.timeout = v;
    return c;
}
