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

/* handler for "check" (src/check.c): token-free backend health check
 * (GET /health + GET /v1/models only — zero tokens, no chat call, no
 * execution rows, no DB writes). `check <model-record-id>` (DB mode,
 * the model row is read for base_url + model_identifier) or
 * `check --base-url <url> --model-identifier <id>` (standalone mode:
 * no DB lookup; `db` is NULL then and must not be touched) */
int cmd_check(cmd_args_t *ga, const global_opts_t *gopts, db_t *db);

/* Single-execution pipeline (src/run.c): claim (start), resolve
 * context/skill/model revisions, preflight (/health, /v1/models),
 * POST /v1/chat/completions, set_raw_response, optional post-hoc
 * output_schema validation, complete/fail — logging an execution_log
 * row per phase. Returns the process exit code. */
int run_execution(db_t *db, int exec_id, int timeout_sec, long max_chars,
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

/* ── API key presence policy ──────────────────────────────────────── */
/* Classify the $OPENAI_API_KEY environment variable, the ONLY API-key
 * source (the --api_key flag is gone and the key is never read from the
 * model configuration blob; see
 * docs/runner_contract.md, decision 4):
 *   NULL (unset)  -> KEY_UNSET_ERR  hard error, the run does not start;
 *   "" (empty)    -> KEY_EMPTY_WARN warning only, no Authorization
 *                     header (acceptable only for a keyless localhost
 *                     server; a bad idea in general);
 *   any other     -> KEY_OK.
 * runner_api_key_message() carries the canonical message, shared by
 * cmd_run (acta_runner) and the GUI runnerWorker so the two cannot
 * drift. */
enum { KEY_OK = 0, KEY_EMPTY_WARN = 1, KEY_UNSET_ERR = 2 };

static inline int runner_api_key_status(const char *key)
{
    if (!key)
        return KEY_UNSET_ERR;
    if (key[0] == '\0')
        return KEY_EMPTY_WARN;
    return KEY_OK;
}

static inline const char *runner_api_key_message(int status)
{
    switch (status) {
    case KEY_UNSET_ERR:
        return "OPENAI_API_KEY is not set; set the environment variable";
    case KEY_EMPTY_WARN:
        return "warning: OPENAI_API_KEY is empty - no Authorization "
               "header will be sent (acceptable only for a keyless "
               "localhost server; a bad idea in general)";
    default:
        return NULL;
    }
}

/* ── check verdict line (one JSON line on stdout) ─────────────────── */
/* Emit the scriptable `check` verdict line on stdout
 * (docs/runner_contract.md, check action):
 *   success: {"ok":true,"model":"<id>","max_context":<n>,"base_url":"<url>"}
 *   failure: {"ok":false,"model":"<id>","base_url":"<url>",
 *             "verdict":"<reason>"}
 * Shared by the `check` action and the run pre-claim auto preflight,
 * so the verdict wording
 * cannot drift between the two surfaces. On success `verdict` is
 * unused; on failure `max_context` is unused. */
static inline void emit_check_verdict(int ok, const char *model,
                                      const char *base_url,
                                      long max_context,
                                      const char *verdict)
{
    if (ok) {
        printf("{\"ok\":true,\"model\":");
        json_str(stdout, model ? model : "");
        printf(",\"max_context\":%ld,\"base_url\":", max_context);
        json_str(stdout, base_url ? base_url : "");
        printf("}\n");
    } else {
        printf("{\"ok\":false,\"model\":");
        json_str(stdout, model ? model : "");
        printf(",\"base_url\":");
        json_str(stdout, base_url ? base_url : "");
        printf(",\"verdict\":");
        json_str(stdout, verdict ? verdict : "");
        printf("}\n");
    }
}

#endif /* ACTA_RUNNER_UTIL_H */
