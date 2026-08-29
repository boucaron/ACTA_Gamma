/* cli_util.h — shared helpers for command handlers */
#ifndef ACTA_DB_CLI_UTIL_H
#define ACTA_DB_CLI_UTIL_H

#include "cli.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>



typedef struct {
    const char *name;
    const char *help;   /* short one-liner shown in the error list */
} action_def_t;

static inline int action_err(const char *entity, const char *action,
                      const action_def_t *actions, size_t count)
{
    /* build "actions":[...] */
    fprintf(stderr,
        "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
        "\"message\":\"unknown %s action: '%s'\",\"actions\":[",
        entity, action);

    /* suggest best prefix match */
    size_t best_len = 0, best_i = 0;
    for (size_t i = 0; i < count; i++) {
        size_t j = 0;
        while (action[j] && actions[i].name[j] && action[j] == actions[i].name[j])
            j++;
        if (j > best_len) { best_len = j; best_i = i; }
    }

    for (size_t i = 0; i < count; i++) {
        if (i) fprintf(stderr, ",");
        if (best_len >= 3 && i == best_i)
            fprintf(stderr, "{\"name\":\"%s\",\"suggested\":true}", actions[i].name);
        else
            fprintf(stderr, "{\"name\":\"%s\"}", actions[i].name);
    }

    fprintf(stderr,
        "],\"usage\":\"actagamma_db %s <action> [flags]\"}\n",
        entity);
    return EXIT_CLI;
}

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

/* ── verbose logging to stderr (levels are cumulative) ────────────── */

/*
 *  Level 0  – silent (default)
 *  Level 1  – action summary        (one line per action)
 *  Level 2  – parameter/field dump  (every input & output field)
 *  Level 3  – raw internal trace    (pointers, raw rc, struct layout)
 *
 * All diagnostics go to stderr so stdout remains pipe-safe.
 */

#define VLOG(lvl, gopts, fmt, ...)                                       \
    do {                                                                 \
        if ((gopts)->verbose >= (lvl)) {                                 \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);    \
        }                                                                \
    } while (0)

#define VERBOSE(filt, lvl, ...)                                          \
    do {                                                                 \
        if ((gopts_local)->verbose >= (lvl)) {                           \
            fprintf(stderr, "[v" #lvl "] " filt, ##__VA_ARGS__);        \
        }                                                                \
    } while (0)

/* Small inline wrapper so call-sites read naturally.
 * The macro needs the local gopts pointer; we alias it at the
 * top of cmd_context and use it throughout. */



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
    int best_dist = 4;
    const char *best = NULL;
    for (size_t i = 0; i < n; i++) {
        int d = edit_distance(input, actions[i].name);
        if (d > 0 && d < best_dist) { best_dist = d; best = actions[i].name; }
    }
    return best;
}


#endif /* ACTA_DB_CLI_UTIL_H */
