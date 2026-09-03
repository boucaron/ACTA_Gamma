#ifndef ACTA_RUNNER_H
#define ACTA_RUNNER_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>


/* ---- version ---- */
#define ACTA_RUNNER_VERSION "0.1.0"

/* ---- exit codes (mirrors acta_db_cli, plus runner-specific codes) ---- */
#define EXIT_OK             0
#define EXIT_NOT_FOUND      1
#define EXIT_SQL            2
#define EXIT_ALLOC          3
#define EXIT_INVALID        4
#define EXIT_CLI            10
#define EXIT_DB_OPEN        11
/* runner-specific (HTTP layer, phase 2):
 * EXIT_HTTP    – the backend call failed (transport / HTTP error)
 * EXIT_TIMEOUT – the backend call timed out */
#define EXIT_HTTP           12
#define EXIT_TIMEOUT        13

/* ---- global options (filled by parse_globals) ----
 *
 * Mirrors the CLI's global_opts_t in spirit: pass 1 extracts the
 * globals, everything else is compacted into argv/argc with argv[0]
 * = action and argv[1..] = positionals + action flags.
 */
typedef struct {
    const char *db;          /* --db */
    int         verbose;     /* -v / --verbose (0–3, cumulative) */
    int         show_version;/* --version */
    int         show_help;   /* --help / -h */

    /* remaining argv after global extraction */
    int   argc;
    char **argv;
} global_opts_t;

/*
 * runner_gopts is defined in src/main.c and set once at dispatch.
 * The runner is one process = one command = single-threaded, so this
 * global is safe and lets helpers log without threading the options
 * pointer through every signature (same pattern as cli_gopts).
 */
extern const global_opts_t *runner_gopts;

/* ---- verbose logging to stderr (levels are cumulative) ----
 *
 *  Level 0  – silent (default)
 *  Level 1  – action summary        (one line per action)
 *  Level 2  – parameter/field dump  (every input & output field)
 *  Level 3  – raw internal trace    (pointers, raw rc, struct layout)
 *
 * All diagnostics go to stderr so stdout remains pipe-safe.
 */
#define VLOG(lvl, fmt, ...)                                              \
    do {                                                                 \
        if (runner_gopts && runner_gopts->verbose >= (lvl)) {            \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);     \
        }                                                                \
    } while (0)

/*
 * Resolve the DB path:
 *   1. --db flag
 *   2. $ACTA_DB
 *   3. ./acta.db
 */
const char *resolve_db_path(const char *flag_db);

/*
 * Runner-layer error emitter (defined in main.c). Emits the canonical
 * single-line JSON error on stderr (same shape as finish_db_error in
 * runner_util.h):
 *   {"error":"ACTA_DB_ERR_<NAME>","code":<rc>,"message":"<escaped>"}
 * `rc` is the raw (negative) ACTA_DB_ERR_* code; the message is
 * formatted then JSON-escaped. Returns the exit code derived from rc
 * (map_rc_to_exit) so the exit code always matches the "code" field;
 * callers must return it.
 */
int runner_error(int rc, const char *fmt, ...);

#endif /* ACTA_RUNNER_H */
