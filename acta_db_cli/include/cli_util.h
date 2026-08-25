/* cli_util.h — shared helpers for command handlers */
#ifndef ACTA_DB_CLI_UTIL_H
#define ACTA_DB_CLI_UTIL_H

#include "cli.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>



typedef struct {
    const char *name;
    const char *help;   /* short one-liner shown in the error list */
} action_def_t;

static int action_err(const char *entity, const char *action,
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



#define VERBOSE(filt, lvl, ...)                                          \
    do {                                                                 \
        if ((gopts_local)->verbose >= (lvl)) {                           \
            fprintf(stderr, "[v" #lvl "] " filt, ##__VA_ARGS__);        \
        }                                                                \
    } while (0)

/* Small inline wrapper so call-sites read naturally.
 * The macro needs the local gopts pointer; we alias it at the
 * top of cmd_context and use it throughout. */





#endif /* ACTA_DB_CLI_UTIL_H */
