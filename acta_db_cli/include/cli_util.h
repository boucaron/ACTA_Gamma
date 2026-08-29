/* cli_util.h — shared helpers for command handlers */
#ifndef ACTA_DB_CLI_UTIL_H
#define ACTA_DB_CLI_UTIL_H

#include "cli.h"
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

/* Map a C API return code → CLI exit code (spec §7.1). */
static inline int map_rc_to_exit(int rc)
{
    switch (rc) {
    case ACTA_DB_OK:            return EXIT_OK;
    case ACTA_DB_ERR_NOT_FOUND: return EXIT_NOT_FOUND;
    case ACTA_DB_ERR_SQL:       return EXIT_SQL;
    case ACTA_DB_ERR_ALLOC:     return EXIT_ALLOC;
    case ACTA_DB_ERR_INVALID:   return EXIT_INVALID;
    default:                    return EXIT_SQL;
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
