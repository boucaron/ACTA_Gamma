#ifndef ACTA_DB_CLI_H
#define ACTA_DB_CLI_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* ---- version ---- */
#define ACTA_DB_CLI_VERSION "0.1.0"

/* ---- exit codes (mirrors spec §7.1) ---- */
#define EXIT_OK             0
#define EXIT_NOT_FOUND      1
#define EXIT_SQL            2
#define EXIT_ALLOC          3
#define EXIT_INVALID        4
#define EXIT_CLI            10
#define EXIT_DB_OPEN        11

/* ---- global options (filled by parse_globals) ---- */
typedef struct {
    /* input / source */
    const char *db;          /* --db */
    const char *fields;      /* --fields */
    const char *json_input;  /* --json */
    int         from_stdin;  /* --stdin */
    const char *from_file;   /* --from_file */

    /* output shaping */
    int         no_nulls;    /* --no_nulls */
    int         id_only;     /* --id_only */
    int         count;       /* --count */
    int         table;       /* --table */
    int         pretty;      /* --pretty */

    /* meta */
    int         show_version;/* --version */
    int         show_help;   /* --help */
    int         show_tools;  /* --tools */

    /* diagnostics */
    int         verbose;     /* -v / --verbose (0–3) */

    /* remaining argv after global extraction */
    int   argc;
    char **argv;
} global_opts_t;

/* ---- verbose logging to stderr (levels are cumulative) ----
 *
 *  Level 0  – silent (default)
 *  Level 1  – action summary        (one line per action)
 *  Level 2  – parameter/field dump  (every input & output field)
 *  Level 3  – raw internal trace    (pointers, raw rc, struct layout)
 *
 * All diagnostics go to stderr so stdout remains pipe-safe.
 *
 * cli_gopts is defined in src/commands.c and set once at dispatch.
 * The CLI is one process = one command = single-threaded, so this
 * global is safe, and it lets helper functions log without threading
 * the options pointer through every signature.
 */
extern const global_opts_t *cli_gopts;

#define VLOG(lvl, fmt, ...)                                              \
    do {                                                                 \
        if (cli_gopts && cli_gopts->verbose >= (lvl)) {                   \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);     \
        }                                                                \
    } while (0)



/* ---- resolved DB path ---- */
const char *resolve_db_path(const char *flag_db);

/* ---- helpers ---- */
/* CLI-layer error emitter (defined in main.c). Emits the canonical
 * single-line JSON error on stderr, enforcing the §7.1 schema exactly
 * once (same shape as finish_db_error in cli_util.h):
 *   {"error":"ACTA_DB_ERR_<NAME>","code":-<exit>,"message":"<escaped>"}
 * `rc` is the raw (negative) ACTA_DB_ERR_* code; the message is
 * formatted then JSON-escaped. The `code` field is the exit code
 * negated (T2 invariant, |code| == exit); the returned exit code is
 * map_rc_to_exit(rc). Callers must return it. */
int cli_error(int rc, const char *fmt, ...);

#endif /* ACTA_CLI_H */
