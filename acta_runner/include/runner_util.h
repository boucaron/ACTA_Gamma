/* runner_util.h — shared helpers (mirrors acta_cli/cli_util.h) */
#ifndef ACTA_RUNNER_UTIL_H
#define ACTA_RUNNER_UTIL_H

#include "runner.h"
#include "argparse.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <db.h>

/* ---- action layer (declared here: needs both runner.h and
 * argparse.h types; every .c file includes this header) ---- */

/*
 * Run the action layer. `action` is the first remaining argv token
 * ("run"); args is the pass-2 view of the action's own arguments.
 * Returns the process exit code. (src/main.c)
 */
int commands_dispatch(const char *action, cmd_args_t *args,
                      const global_opts_t *gopts, db_t *db);

/* handler for "run" (src/run.c) */
int cmd_run(cmd_args_t *ga, const global_opts_t *gopts, db_t *db);

/* handler for "sweep" (src/sweep.c): fails `running` executions whose
 * last execution_log activity is older than --stale-seconds (R4,
 * dead-runner cleanup per decision 6) */
int cmd_sweep(cmd_args_t *ga, const global_opts_t *gopts, db_t *db);

/* Single-execution pipeline (src/run.c): claim (start), resolve
 * context/skill/model revisions, preflight (/health, /v1/models),
 * POST /v1/chat/completions, set_raw_response, optional post-hoc
 * output_schema validation, complete/fail — logging an execution_log
 * row per phase. Returns the process exit code. */
int run_execution(db_t *db, int exec_id, int timeout_sec,
                  const char *api_key);

/* Parse a positive-integer id ("42") from a CLI argument.
 * Strict: rejects trailing garbage ("42abc"), non-numeric ("abc"),
 * zero, negatives, and overflow via strtol+endptr.
 * Returns 1 on success (stored into *out); 0 on invalid input. */
static inline int parse_positive_id(const char *s, int *out)
{
    if (!s || !out) return 0;
    errno = 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (errno == ERANGE || *end != '\0' || v <= 0 || v > (long)INT_MAX)
        return 0;
    *out = (int)v;
    return 1;
}

/* Parse a non-negative integer ("0" is valid) for --max/--timeout.
 * Same strict rules as parse_positive_id but 0 is accepted. */
static inline int parse_nonneg_int(const char *s, int *out)
{
    if (!s || !out) return 0;
    errno = 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (errno == ERANGE || *end != '\0' || v < 0 || v > (long)INT_MAX)
        return 0;
    *out = (int)v;
    return 1;
}

/* Map a C API return code → runner exit code. Same mapping as the
 * CLI: INVALID_DB, DUPLICATE and FK are constraint/validation
 * failures, not SQL errors, so they exit EXIT_INVALID. */
static inline int map_rc_to_exit(int rc)
{
    switch (rc) {
    case ACTA_DB_OK:             return EXIT_OK;
    case ACTA_DB_ERR_NOT_FOUND:  return EXIT_NOT_FOUND;
    case ACTA_DB_ERR_SQL:        return EXIT_SQL;
    case ACTA_DB_ERR_ALLOC:      return EXIT_ALLOC;
    case ACTA_DB_ERR_INVALID:    return EXIT_INVALID;
    case ACTA_DB_ERR_INVALID_DB:
    case ACTA_DB_ERR_DUPLICATE:
    case ACTA_DB_ERR_FK:         return EXIT_INVALID;
    default:                     return EXIT_INVALID;
    }
}

/* ── JSON string escaping ─────────────────────────────────────────── */
static inline void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        switch (*s) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f);  break;
        case '\r': fputs("\\r", f);  break;
        case '\t': fputs("\\t", f);  break;
        default:
            if ((unsigned char)*s < 0x20)
                fprintf(f, "\\u%04x", (unsigned char)*s);
            else
                fputc(*s, f);
        }
    }
    fputc('"', f);
}

/* ── Single-line JSON error contract (stderr) ─────────────────────── */
/* Emit the canonical single-line JSON error on stderr:
 *   {"error":"ACTA_DB_ERR_<NAME>","code":<rc>,"message":"<what>"}
 * and return map_rc_to_exit(rc). `what` is JSON-escaped and NULL-safe.
 * `rc` should be a negative ACTA_DB_ERR_* code; ACTA_DB_OK or an
 * unknown code is reported as ACTA_DB_ERR_INVALID. stderr line 1 is
 * the contract that scripts parse. */
static inline int finish_db_error(int rc, const char *what)
{
    static const struct { int code; const char *name; } names[] = {
        { ACTA_DB_ERR_NOT_FOUND,  "ACTA_DB_ERR_NOT_FOUND"  },
        { ACTA_DB_ERR_SQL,        "ACTA_DB_ERR_SQL"        },
        { ACTA_DB_ERR_ALLOC,      "ACTA_DB_ERR_ALLOC"      },
        { ACTA_DB_ERR_INVALID,    "ACTA_DB_ERR_INVALID"    },
        { ACTA_DB_ERR_INVALID_DB, "ACTA_DB_ERR_INVALID_DB" },
        { ACTA_DB_ERR_DUPLICATE,  "ACTA_DB_ERR_DUPLICATE"  },
        { ACTA_DB_ERR_FK,         "ACTA_DB_ERR_FK"         },
    };
    const char *name = NULL;
    if (rc != ACTA_DB_OK) {
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
            if (names[i].code == rc) { name = names[i].name; break; }
    }
    if (!name) { rc = ACTA_DB_ERR_INVALID; name = "ACTA_DB_ERR_INVALID"; }
    fprintf(stderr, "{\"error\":\"%s\",\"code\":%d,\"message\":",
            name, rc);
    json_str(stderr, what ? what : "");
    fputs("}\n", stderr);
    return map_rc_to_exit(rc);
}

/* Emit the JSON error line for a failed library call and return the
 * mapped exit code. `op` names the operation; the detail is
 * acta_db_last_error(db):
 *   {"error":"ACTA_DB_ERR_*","code":<rc>,"message":"<op> failed: <detail>"}
 */
static inline int finish_op_error(db_t *db, int rc, const char *op)
{
    const char *msg = (db != NULL) ? acta_db_last_error(db) : NULL;
    char what[512];
    snprintf(what, sizeof what, "%s failed: %s",
             op ? op : "operation", msg ? msg : "(no detail)");
    return finish_db_error(rc, what);
}

/* Emit the canonical invalid-argument error line on stderr (pure
 * emitter, no exit code):
 *   {"error":"ACTA_DB_ERR_INVALID","code":-4,"message":"<msg>"}
 */
static inline void emit_error(const char *msg)
{
    fprintf(stderr,
        "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
        "\"message\":");
    json_str(stderr, msg ? msg : "");
    fputs("}\n", stderr);
}

/* Emit the canonical not-found error line and return EXIT_NOT_FOUND:
 *   {"error":"ACTA_DB_ERR_NOT_FOUND","code":-1,"message":"<entity> not found"}
 */
static inline int emit_not_found(const char *entity)
{
    char what[128];
    snprintf(what, sizeof what, "%s not found", entity ? entity : "entity");
    return finish_db_error(ACTA_DB_ERR_NOT_FOUND, what);
}

/* Emit the runner-layer error line for non-DB failures (HTTP, timeout,
 * validation, claim, ...). Same single-line contract shape as the DB
 * errors, but `code` is the runner exit code, so it always matches the
 * process exit code:
 *   {"error":"ACTA_RUNNER_ERROR","code":<exit_code>,"message":"<msg>"}
 */
static inline int emit_runner_error(int exit_code, const char *msg)
{
    fprintf(stderr,
        "{""error\":\"ACTA_RUNNER_ERROR\",\"code\":%d,"
        "\"message\":",
        exit_code);
    json_str(stderr, msg ? msg : "");
    fputs("}\n", stderr);
    return exit_code;
}

#endif /* ACTA_RUNNER_UTIL_H */
