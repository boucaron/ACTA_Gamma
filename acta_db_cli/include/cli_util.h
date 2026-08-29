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
    int best_dist = 4;
    const char *best = NULL;
    for (size_t i = 0; i < n; i++) {
        int d = edit_distance(input, actions[i].name);
        if (d > 0 && d < best_dist) { best_dist = d; best = actions[i].name; }
    }
    return best;
}


#endif /* ACTA_DB_CLI_UTIL_H */
