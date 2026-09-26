#include "runnerWorker.h"
#include "dbhandle.h"

#include <QByteArray>

#include <cstdlib>

// Exit codes for the pipeline result (macros, no linkage).
#include "runner.h"

// Pure C headers: runner_util.h declares the in-process pipeline
// (run_execution) and conf.h the shared per-machine config-file
// helpers (default path, API key precedence policy, built-in defaults)
// used by the CLI runner's cmd_run (docs/runner_contract.md, decision 4).
// Both are C, so they are included under C linkage.
extern "C" {
#include "runner_util.h"
#include "conf.h"
void backend_cancel_request(void);
void backend_cancel_reset(void);
}

// runner_gopts is declared in runner.h and used by VLOG in run.c/
// backend.c, but is only *defined* in acta_runner's main.c, which the
// app does not link. Defining it as nullptr here makes every VLOG
// call a no-op (the macro guards on a non-null pointer).
const global_opts_t *runner_gopts = nullptr;

#include "confreader.h"  // Qt config-file reader (shared with
                         // MainWindow::defaultDbPath)

// Message of the execution's last error-level log line (the terminal
// "execution_failed" row on failure); empty when there are no rows.
static QString lastErrorLogMessage(db_t *db, int executionId)
{
    int n = 0;
    int err = ACTA_DB_OK;
    execution_log_t **lines =
        acta_db_execution_log_list_by_execution(db, executionId, nullptr,
                                                0, 0, &n, &err);
    if (!lines || n == 0)
        return QString();
    const QString message = lines[n - 1]->message
        ? QString::fromUtf8(lines[n - 1]->message)
        : QString();
    acta_db_execution_log_list_free(lines, n);
    return message;
}

RunnerWorker::RunnerWorker(int executionId, const QString &dbPath,
                           QObject *parent)
    : QObject(parent),
      m_executionId(executionId),
      m_dbPath(dbPath)
{
}

void RunnerWorker::requestCancel()
{
    backend_cancel_request();
}

void RunnerWorker::resetCancel()
{
    backend_cancel_reset();
}

void RunnerWorker::runInThread()
{
    // A stale flag must not leak into this run (the flag is a process
    // global shared by all worker runs).
    resetCancel();

    // Guard against an empty path: sqlite3_open("") would silently open
    // an empty database and surface as a misleading "invalid sqlitedb
    // file" error.
    if (m_dbPath.isEmpty()) {
        Q_EMIT finished(EXIT_INVALID,
                        tr("No database path is available; reconnect the "
                           "database first."));
        return;
    }

    // Own connection, opened inside the thread (see header comment).
    int err = ACTA_DB_OK;
    db_t *db = acta_db_open(m_dbPath.toUtf8().constData(), &err,
                            ACTA_DB_OPEN_EXISTING);
    if (!db) {
        const QString detail =
            QString::fromUtf8(acta_db_strerror(err));
        Q_EMIT finished(EXIT_INVALID,
                        tr("Cannot open database '%1': %2")
                            .arg(m_dbPath, detail));
        return;
    }

    // Per-machine settings resolution
    // (docs/runner_contract.md, decisions 4, 5, and 8): the same
    // resolution as the runner's cmd_run, through the shared helper.
    // The GUI has no --timeout flag and no separate GUI config, so the
    // file supplies both values, falling back to the built-in defaults:
    //   api_key:   $OPENAI_API_KEY (if set) -> config file "api_key";
    //              the file is a fallback, not a second channel.
    //   timeout:   config file "timeout" -> built-in default (600 s);
    //              the file supplies the default, never a per-run
    //              override.
    //   max_chars: config file "max_chars" -> built-in default
    //              (100,000 chars); passed into the pipeline
    //              (run_execution limit parameter) and consumed by the
    //              preflight size check
    //              (docs/runner_contract.md, decision 8).
    // A missing or unreadable file is simply unavailable as a fallback;
    // a readable-but-malformed file (or one that fails the POSIX
    // permission gate) is a fail-closed hard error before any claim.
    const ActaConfFile conf = readActaConfFile(
        QString::fromUtf8(acta_conf_default_path()));
    if (!conf.valid) {
        Q_EMIT finished(EXIT_INVALID, conf.error);
        resetCancel();
        return;
    }

    // The pipeline never reads the key from the model configuration
    // blob (docs/runner_contract.md, decision 4). Presence policy via
    // the shared helper: unset -> hard error, the pipeline is not
    // started; empty -> warning in the run result, no Authorization
    // header. The Qt-parsed key is passed as file_key; an absent key
    // is NULL, a present-but-empty key is "" (the policy distinguishes
    // the two).
    const char *envKey = std::getenv("OPENAI_API_KEY");
    QByteArray fileKeyBytes =
        conf.apiKeyPresent ? conf.apiKey.toUtf8() : QByteArray();
    const char *fileKey =
        conf.apiKeyPresent ? fileKeyBytes.constData() : nullptr;
    const char *keyMsg = nullptr;
    const int keyStatus = acta_conf_api_key_status(envKey, fileKey, &keyMsg);
    if (keyStatus == ACTA_KEY_UNSET_ERR) {
        Q_EMIT finished(EXIT_INVALID, QString::fromUtf8(keyMsg));
        resetCancel();
        return;
    }
    // A set (even empty) env var shadows a non-empty config file
    // "api_key": surface the same warning acta_runner prints to stderr
    // (docs/runner_contract.md, decision 4) in the run result message.
    const char *shadowMsg = nullptr;
    acta_conf_api_key_shadow_warning(envKey, fileKey, &shadowMsg);
    const char *apiKey = (envKey != NULL) ? envKey : fileKey;

    const int timeoutSec = (conf.timeout > 0)
        ? (int)conf.timeout
        : ACTA_CONF_DEFAULT_TIMEOUT;
    const long maxChars = (conf.maxChars > 0)
        ? conf.maxChars
        : (long)ACTA_CONF_DEFAULT_MAX_CHARS;

    const int exitCode =
        run_execution(db, m_executionId, timeoutSec, maxChars, apiKey);
    resetCancel(); // don't leak the flag into the next run
    QString message;
    if (exitCode == 0) {
        if (keyStatus == ACTA_KEY_EMPTY_WARN)
            message = QString::fromUtf8(keyMsg);
        if (shadowMsg)
            message = message.isEmpty()
                ? QString::fromUtf8(shadowMsg)
                : message + " " + QString::fromUtf8(shadowMsg);
    } else {
        message = lastErrorLogMessage(db, m_executionId);
    }

    int closeRc = acta_db_close(db);
    if (closeRc != ACTA_DB_OK)
        acta_db_force_close(db);

    Q_EMIT finished(exitCode, message);
}
