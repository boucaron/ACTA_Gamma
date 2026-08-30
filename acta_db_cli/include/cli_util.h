/* cli_util.h — shared helpers for command handlers */
#ifndef ACTA_DB_CLI_UTIL_H
#define ACTA_DB_CLI_UTIL_H

#include "cli.h"
#include "argparse.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <db.h>



typedef struct {
    const char *name;
    const char *help;   /* short one-liner shown in the error list */
} action_def_t;

/* Parse a positive-integer id ("42") from a CLI argument.
 * Strict: rejects trailing garbage ("42abc"), non-numeric ("abc"),
 * zero, negatives, and overflow ("999999999999999") via strtol+endptr
 * instead of atoi (which accepts "42abc" and has UB on overflow).
 * Returns 1 on success (and stores into *out); 0 on invalid input
 * (*out left unmodified). */
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

/* Parse a non-negative integer ("0" is valid) for --offset/--limit.
 * Same strict rules as parse_positive_id (strtol+endptr, ERANGE,
 * INT_MAX) but 0 is accepted. Returns 1 on success, 0 on invalid
 * input (*out left unmodified). */
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

/* Parse an optional folder/parent id: strict format, same rules as
 * parse_nonneg_int (strtol+endptr, ERANGE, INT_MAX). 0 = root / no
 * parent. Negatives are REJECTED (user typo) rather than clamped to
 * root, so --folder_id -3 is an error, not a silent move to root.
 * Returns 1 on success, 0 on invalid format (*out left unmodified). */
static inline int parse_folder_id(const char *s, int *out)
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

/* Map a C API return code → CLI exit code (spec §7.1). The rc set is
 * complete (db.h): INVALID_DB, DUPLICATE and FK are constraint /
 * validation failures, not SQL errors, so they exit EXIT_INVALID.
 * The default is an explicit generic fallback for unknown rc values —
 * never EXIT_SQL (an unknown code is not evidence of a SQL error). */
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

/* Does the comma-separated field list contain "name"? */
static inline int fields_has(const char *fields, const char *name)
{
    if (!fields) return 0;
    size_t nlen = strlen(name);
    const char *p = fields;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len == nlen && strncmp(p, name, len) == 0)
            return 1;
        p = comma ? comma + 1 : p + len;
    }
    return 0;
}

/* Emit a truncated column for --table (max `width` chars, space-padded). */
static inline void tcol(FILE *f, const char *s, int width)
{
    if (!s) s = "-";
    char buf[64];
    int len = (int)strlen(s);
    int n = len > width ? width : len;
    if (n < 0) n = 0;                        /* negative width guard */
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;  /* buf overflow guard */
    memcpy(buf, s, n);
    buf[n] = '\0';
    fprintf(f, "%-*s ", width, buf);
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
 * and return map_rc_to_exit(rc).  `what` is JSON-escaped via json_str
 * and NULL-safe.  `rc` should be a negative ACTA_DB_ERR_* code;
 * ACTA_DB_OK or an unknown code is reported as ACTA_DB_ERR_INVALID.
 * Callers may print human/usage text AFTER the JSON line — stderr
 * line 1 is the contract that scripts parse.  This is the centralized
 * error emitter for library-failure paths (P4 #6); entity files adopt
 * it incrementally. */
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

/* Emit the JSON error line for a failed library call and return the mapped
 * exit code.  `op` names the operation ("model update", "skill create", …);
 * the detail is `acta_db_last_error(db)`, so the stderr line is
 *   {"error":"ACTA_DB_ERR_*","code":<rc>,"message":"<op> failed: <detail>"}
 * Thin wrapper over finish_db_error for the per-entity
 * `if (rc != ACTA_DB_OK)` paths (P4 #3) — the single place that composes
 * the "<op> failed: <detail>" message. */
static inline int finish_op_error(db_t *db, int rc, const char *op)
{
    const char *msg = (db != NULL) ? acta_db_last_error(db) : NULL;
    char what[512];
    snprintf(what, sizeof what, "%s failed: %s",
             op ? op : "operation", msg ? msg : "(no detail)");
    return finish_db_error(rc, what);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  S1/V1 common atoms — the six re-typed units shared by every        */
/*  entity file.  Entities adopt them opt-in, site by site; each       */
/*  atom replaces one hand-rolled block with a one-liner.              */
/* ══════════════════════════════════════════════════════════════════ */

/* ── error / success emitters (stdout-schema strings live here — S3) ─ */

/* Emit the canonical invalid-argument error line on stderr:
 *   {"error":"ACTA_DB_ERR_INVALID","code":-4,"message":"<msg>"}
 * Pure emitter (no exit code): callers keep their own control flow and
 * return EXIT_INVALID.  `msg` is JSON-escaped via json_str. */
static inline void emit_error(const char *msg)
{
    fprintf(stderr,
        "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4," 
        "\"message\":");
    json_str(stderr, msg ? msg : "");
    fputs("}\n", stderr);
}

/* Emit the canonical not-found error line on stderr:
 *   {"error":"ACTA_DB_ERR_NOT_FOUND","code":-5,"message":"<entity> not found"}
 * and return EXIT_NOT_FOUND (callers: `return emit_not_found("model");`).
 * Same bytes as finish_db_error(ACTA_DB_ERR_NOT_FOUND, "<entity> not found"). */
static inline int emit_not_found(const char *entity)
{
    char what[128];
    snprintf(what, sizeof what, "%s not found", entity ? entity : "entity");
    return finish_db_error(ACTA_DB_ERR_NOT_FOUND, what);
}

/* Emit a success id on stdout.  Honours --id_only (bare `N`) vs the
 * default JSON wrapper ({"id":N}). */
static inline void emit_ok_id(const global_opts_t *g, int id)
{
    if (g && g->id_only)
        fprintf(stdout, "%d\n", id);
    else
        fprintf(stdout, "{\"id\":%d}\n", id);
}

/* Emit a move success on stdout: {"id":N,"folder_id":M} (or bare N with
 * --id_only).  folder_id is the raw int (0 = root) — the S3 root-folder
 * wire question (null vs 0) is decided per entity, not here. */
static inline void emit_ok_folder(const global_opts_t *g, int id, int folder_id)
{
    if (g && g->id_only)
        fprintf(stdout, "%d\n", id);
    else
        fprintf(stdout, "{\"id\":%d,\"folder_id\":%d}\n", id, folder_id);
}

/* Emit a delete success on stdout: {"deleted":true}. */
static inline void emit_deleted(void)
{
    fputs("{\"deleted\":true}\n", stdout);
}

/* ── input atoms ──────────────────────────────────────────────────── */

/* Consume the id positional from the command iterator.
 * Returns 1 on success (id stored in *out), 0 on error — the error
 * (JSON line + per-action usage) is already on stderr; callers do
 * `if (!parse_id_positional(ga, "id", usage_get, "model get", &id))
 *  return EXIT_INVALID;`.
 * `pos` is the documented positional name ("id", "model_id", …); the
 * messages are canonical:
 *   "missing positional: <pos>" / "invalid <pos>: must be a positive
 *   integer". */
static inline int parse_id_positional(cmd_args_t *ga, const char *pos,
                                      void (*usage)(FILE *),
                                      const char *vlog_label, int *out)
{
    const char *s = cmd_args_next_positional(ga);
    char msg[96];
    if (!s) {
        VLOG(1, "%s: ERROR missing <%s>", vlog_label, pos);
        snprintf(msg, sizeof msg, "missing positional: <%s>", pos);
        emit_error(msg);
        usage(stderr);
        return 0;
    }
    if (!parse_positive_id(s, out)) {
        VLOG(1, "%s: invalid id=%s", vlog_label, s);
        snprintf(msg, sizeof msg, "invalid <%s>: must be a positive integer",
                 pos);
        emit_error(msg);
        usage(stderr);
        return 0;
    }
    return 1;
}

/* Read a non-negative-integer flag from the command iterator.
 * Returns 1 = present (*out set), 0 = absent (*out untouched),
 * -1 = error (JSON line + usage already on stderr; callers return
 * EXIT_INVALID).  `required` = 1 turns absence into the "missing
 * required flag: --<name>" error.  The invalid-value message is
 * canonical: "--<name> must be a non-negative integer". */
static inline int parse_nonneg_int_flag(cmd_args_t *ga, const char *name,
                                        int *out, int required,
                                        void (*usage)(FILE *),
                                        const char *vlog_label)
{
    const char *v = cmd_args_flag(ga, name, 1);
    char msg[96];
    if (!v) {
        if (!required) return 0;
        VLOG(1, "%s: ERROR missing --%s", vlog_label, name);
        snprintf(msg, sizeof msg, "missing required flag: --%s", name);
        emit_error(msg);
        usage(stderr);
        return -1;
    }
    {
        int val;
        if (!parse_nonneg_int(v, &val)) {
            VLOG(1, "%s: ERROR --%s must be a non-negative integer, got '%s'",
                 vlog_label, name, v);
            snprintf(msg, sizeof msg,
                     "--%s must be a non-negative integer", name);
            emit_error(msg);
            usage(stderr);
            return -1;
        }
        *out = val;
        return 1;
    }
}

/* Read the optional --offset / --limit pagination pair (both default 0).
 * Thin composition over parse_nonneg_int_flag.  Returns 0 on success
 * (both stored), -1 on error (JSON line + usage already on stderr;
 * callers return EXIT_INVALID). */
static inline int parse_offset_limit(cmd_args_t *ga, int *offset, int *limit,
                                     void (*usage)(FILE *),
                                     const char *vlog_label)
{
    if (parse_nonneg_int_flag(ga, "offset", offset, 0, usage, vlog_label) < 0)
        return -1;
    return parse_nonneg_int_flag(ga, "limit", limit, 0, usage, vlog_label);
}

/* Read a string flag and require a non-empty value when present.
 * Returns 1 = present + non-empty (*out_val = value), 0 = absent
 * (*out_val = NULL, not an error), -1 = present but empty (JSON line
 * "field '<name>' must not be empty" + usage already on stderr;
 * callers return EXIT_INVALID). */
static inline int require_flag(cmd_args_t *ga, const char *name,
                               const char **out_val,
                               void (*usage)(FILE *),
                               const char *vlog_label)
{
    const char *v = cmd_args_flag(ga, name, 1);
    char msg[96];
    if (!v) { *out_val = NULL; return 0; }
    if (!*v) {
        VLOG(1, "%s: ERROR --%s must not be empty", vlog_label, name);
        snprintf(msg, sizeof msg, "field '%s' must not be empty", name);
        emit_error(msg);
        usage(stderr);
        return -1;
    }
    *out_val = v;
    return 1;
}

/* Post-fetch check + emit: the not-found unit the P3 fix had to land
 * in every entity file.  `err`/`row` come from the entity's fetch call
 * (already performed — fetch signatures vary per entity); `free_row`
 * frees the row on the library-error path (may be passed a cast
 * `acta_db_<e>_free`).  Returns 0 on success (row stays owned by the
 * caller); on a library error the row is freed, the finish_op_error
 * JSON line is emitted, and the mapped exit code is returned; on a
 * NULL row the not-found line is emitted and EXIT_NOT_FOUND returned.
 * Callers: `if ((rc = load_row_or_notfound(…))) return rc;`. */
static inline int load_row_or_notfound(db_t *db, int err, void *row, int id,
                                       void (*free_row)(void *),
                                       const char *op, const char *entity)
{
    if (err != ACTA_DB_OK) {
        VLOG(1, "  FAILED err=%d → exit mapping", err);
        free_row(row);
        return finish_op_error(db, err, op);
    }
    if (!row) {
        VLOG(1, "  not found (id=%d)", id);
        return emit_not_found(entity);
    }
    return 0;
}

/* Verbose logging: VLOG() — single definition in cli.h. */



// Check for commands

/* ══════════════════════════════════════════════════════════════════ */
/*  Fuzzy-matching helpers (for "did you mean …?")                    */
/* ══════════════════════════════════════════════════════════════════ */
static inline int edit_distance(const char *a, const char *b)
{
    int la = (int)strlen(a);
    int lb = (int)strlen(b);

    /* Inputs are only used for fuzzy "did you mean" suggestions; anything
     * this long can never be within edit distance 4 of a real action name,
     * so bail out before touching the 64x64 DP buffer below. */
    if (la > 60 || lb > 60) return 61;

    int dp[64][64];

    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;

    for (int i = 1; i <= la; i++) {
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int best = dp[i-1][j] + 1;          /* deletion   */
            if (dp[i][j-1] + 1 < best)           /* insertion  */
                best = dp[i][j-1] + 1;
            if (dp[i-1][j-1] + cost < best)      /* substitution*/
                best = dp[i-1][j-1] + cost;
            dp[i][j] = best;
        }
    }
    return dp[la][lb];
}

static inline const char *closest_action(const char *input,
                                  const action_def_t *actions, size_t n)
{
    if (!input || !*input) return NULL;
    const char *best = NULL;
    int best_dist = INT_MAX;
    for (size_t i = 0; i < n; i++) {
        const char *name = actions[i].name;
        /* Length-relative acceptance threshold (per target name): 1 edit
         * is meaningful, 3+ never are. Short names (≤ 3 chars) get only
         * 1, so "cat" does not suggest "get" (d=2) while "creat"→
         * "create" (d=1) and "cretae"→"create" (d=2) still fire. */
        int limit = (int)strlen(name) <= 3 ? 1 : 2;
        int d = edit_distance(input, name);
        if (d > 0 && d <= limit && d < best_dist) { best_dist = d; best = name; }
    }
    return best;
}

/* Emit the canonical unknown-action error for an entity and return its
 * exit code. Single implementation for all entities (the previous per-file
 * copy-paste had drifted: 2 of 10 files logged the wrong VLOG label / help
 * pointer). `vlog_label` and `help_target` are passed by the caller, not
 * derived, and the stderr text is a fixed contract (scripts grep for
 * "Unknown action") — output stays byte-stable, historical quirks
 * included. */
static inline int unknown_action(const char *vlog_label, const char *action,
                                 const char *help_target,
                                 const action_def_t *actions, size_t n)
{
    const char *shown = action ? action : "(null)";
    const char *guess = closest_action(action, actions, n);

    VLOG(1, "%s: unknown action '%s'%s",
         vlog_label, shown, guess ? "  (suggestion below)" : "");

    fprintf(stderr, "Unknown action '%s'.\n", shown);
    if (guess)
        fprintf(stderr, "  Did you mean '%s'?\n", guess);
    fprintf(stderr, "  Run '%s' for full usage.\n", help_target);
    return EXIT_INVALID;
}


#endif /* ACTA_DB_CLI_UTIL_H */
