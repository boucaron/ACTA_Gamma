#ifndef ACTA_DEATHMARK_H
#define ACTA_DEATHMARK_H

#include "acta_db.h"

/*
 * Death marker for orphaned `running` rows.
 *
 * On a clean process exit (SIGINT / SIGTERM / atexit) while an
 * execution is claimed in `running`, the marker transitions the row to
 * `failed` with the diagnostic
 *   "runner process exited during execution"
 * and appends the matching `execution_failed` phase row, instead of
 * leaving the row orphaned in `running`.
 *
 * Scope: the runner claims per-execution and `run --pending` is
 * sequential, so at most one row is `running` at a time — one marker
 * path, no bookkeeping.  A hard kill (SIGKILL, power loss) still
 * orphans the row; `sweep` remains the recovery path for that case.
 */

/* Record the claimed execution (its db handle + id).  Called right
 * after the atomic pending -> running claim succeeds. */
void deathmark_claim(db_t *db, int exec_id);

/* Forget the claimed execution (the row has left `running`). */
void deathmark_release(void);

/* The whole marker, as plain testable C: mark `exec_id` `failed` with
 * the diagnostic + the matching `execution_failed` log row, but only
 * if the row is still `running` (a normal completion / failure /
 * cancel already transitioned it).  Returns 1 when marked, 0 when
 * nothing was done.  Idempotent. */
int deathmark_mark(db_t *db, int exec_id);

/* The exit path (the atexit hook body): runs the marker on the
 * currently claimed execution.  Testable in-process. */
void deathmark_exit_path(void);

/* Install the exit path: an atexit hook plus SIGINT / SIGTERM handlers
 * that terminate via exit() so the atexit hook runs the marker.  The
 * wiring is deliberately thin around deathmark_mark. */
void deathmark_install(void);

#endif /* ACTA_DEATHMARK_H */
