#include "runnerWorker.h"
#include "dbhandle.h"

#include <cstdlib>

// Exit codes for the pipeline result (macros, no linkage).
#include "runner.h"

// runner_gopts is declared in runner.h and used by VLOG in run.c/
// backend.c, but is only *defined* in acta_runner's main.c, which the
// app does not link. Defining it as nullptr here makes every VLOG
// call a no-op (the macro guards on a non-null pointer).
const global_opts_t *runner_gopts = nullptr;

// The in-process pipeline (defined in acta_runner/src/run.c, compiled
// into this app by src.pro). Same contract as the CLI's "run" action:
// claim -> resolve -> preflight -> chat call -> record -> validate ->
// complete/fail, one execution_log row per phase. runner_util.h is a
// pure C header (incl. the API key presence policy helpers shared with
// the CLI runner), so it is included under C linkage.
extern "C" {
#include "runner_util.h"
int run_execution(db_t *db, int executionId, int timeoutSec,
                   const char *apiKey);
void backend_cancel_request(void);
void backend_cancel_reset(void);
}

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
                           int timeoutSec, QObject *parent)
    : QObject(parent),
      m_executionId(executionId),
      m_dbPath(dbPath),
      m_timeoutSec(timeoutSec)
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

    // API key: $OPENAI_API_KEY only — the UI has no --api_key flag, and
    // the pipeline never reads the key from the model configuration
    // blob (docs/plans/drop-runner-api-key-flag.md). Presence policy: unset
    // -> hard error, the pipeline is not started; empty -> warning in
    // the run result, no Authorization header.
    const char *apiKey = std::getenv("OPENAI_API_KEY");
    if (runner_api_key_status(apiKey) == KEY_UNSET_ERR) {
        Q_EMIT finished(EXIT_INVALID,
                        tr("%1").arg(QString::fromUtf8(
                            runner_api_key_message(KEY_UNSET_ERR))));
        resetCancel();
        return;
    }
    const int exitCode =
        run_execution(db, m_executionId, m_timeoutSec, apiKey);
    resetCancel(); // don't leak the flag into the next run
    QString message;
    if (exitCode == 0) {
        if (runner_api_key_status(apiKey) == KEY_EMPTY_WARN)
            message = tr("%1").arg(QString::fromUtf8(
                runner_api_key_message(KEY_EMPTY_WARN)));
    } else {
        message = lastErrorLogMessage(db, m_executionId);
    }

    int closeRc = acta_db_close(db);
    if (closeRc != ACTA_DB_OK)
        acta_db_force_close(db);

    Q_EMIT finished(exitCode, message);
}
