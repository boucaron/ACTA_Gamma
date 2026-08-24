#include "argparse.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  helpers                                                            */
/* ------------------------------------------------------------------ */

static int is_flag(const char *s) {
    return s[0] == '-' && s[1] == '-';
}

/* does token look like --name or --name=value ? */
static int flag_prefix_match(const char *token, const char *name) {
    size_t len = strlen(name);
    if (strncmp(token, "--", 2) != 0) return 0;
    if (strncmp(token + 2, name, len) != 0) return 0;
    return token[2 + len] == 0 || token[2 + len] == '=';
}

/* ------------------------------------------------------------------ */
/*  parse_globals                                                      */
/* ------------------------------------------------------------------ */

int parse_globals(int argc, char **argv, global_opts_t *g) {
    memset(g, 0, sizeof(*g));

    /* temp buffer for non-global args */
    char **rest = malloc(sizeof(char *) * (size_t)argc);
    if (!rest) return EXIT_CLI;
    int rest_n = 0;

    for (int i = 1; i < argc; i++) {
        char *a = argv[i];

        /* ---- --version ---- */
        if (strcmp(a, "--version") == 0) {
            free(rest);
            g->show_version = 1;
            g->argc = 0;
            g->argv = NULL;
            return 0;
        }
        /* ---- --help ---- */
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            free(rest);
            g->show_help = 1;
            g->argc = 0;
            g->argv = NULL;
            return 0;
        }
        /* ---- --tools ---- */
        if (strcmp(a, "--tools") == 0) {
            free(rest);
            g->show_tools = 1;
            g->argc = 0;
            g->argv = NULL;
            return 0;
        }
        /* ---- --db <path> ---- */
        if (flag_prefix_match(a, "db")) {
            if (a[4] == '=') {
                g->db = a + 5;
            } else {
                if (i + 1 >= argc) { free(rest); return EXIT_CLI; }
                g->db = argv[++i];
            }
            continue;
        }
        /* ---- --fields <f1,f2> ---- */
        if (flag_prefix_match(a, "fields")) {
            if (a[8] == '=') g->fields = a + 9;
            else {
                if (i + 1 >= argc) { free(rest); return EXIT_CLI; }
                g->fields = argv[++i];
            }
            continue;
        }
        /* ---- --no-nulls ---- */
        if (strcmp(a, "--no-nulls") == 0) { g->no_nulls = 1; continue; }
        /* ---- --id-only ---- */
        if (strcmp(a, "--id-only") == 0)  { g->id_only = 1;  continue; }
        /* ---- --count ---- */
        if (strcmp(a, "--count") == 0)    { g->count = 1;    continue; }
        /* ---- --table ---- */
        if (strcmp(a, "--table") == 0)    { g->table = 1;    continue; }
        /* ---- --pretty ---- */
        if (strcmp(a, "--pretty") == 0)   { g->pretty = 1;   continue; }
        /* ---- --verbose / -v (stackable, 0–3) ---- */
        if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            if (g->verbose < 3) g->verbose++;
            continue;
        }
        if (flag_prefix_match(a, "verbose")) {
            /* --verbose=N form (explicit level) */
            if (a[9] == '=') {
                int lvl = atoi(a + 10);
                g->verbose = (lvl >= 1 && lvl <= 3) ? lvl : 3;
            } else {
                /* --verbose <N> (space form, treat N as level) */
                if (i + 1 < argc) {
                    int lvl = atoi(argv[i + 1]);
                    if (lvl >= 1 && lvl <= 3) g->verbose = lvl;
                    else g->verbose = 3;
                    i++;
                } else if (g->verbose < 3) {
                    g->verbose++;
                }
            }
            continue;
        }
        /* ---- --json <blob> ---- */
        if (flag_prefix_match(a, "json")) {
            if (a[6] == '=') g->json_input = a + 7;
            else {
                if (i + 1 >= argc) { free(rest); return EXIT_CLI; }
                g->json_input = argv[++i];
            }
            continue;
        }
        /* ---- --stdin ---- */
        if (strcmp(a, "--stdin") == 0)    { g->from_stdin = 1;  continue; }
        /* ---- --from-file <path> ---- */
        if (flag_prefix_match(a, "from-file")) {
            if (a[11] == '=') g->from_file = a + 12;
            else {
                if (i + 1 >= argc) { free(rest); return EXIT_CLI; }
                g->from_file = argv[++i];
            }
            continue;
        }
        /* ---- --create-dirs (spec §3, not global per spec but harmless) ---- */
        if (strcmp(a, "--create-dirs") == 0) { /* swallow; TODO: thread to open */ continue; }

        /* not a recognised global → keep as entity/action/positional */
        rest[rest_n++] = a;
    }

    g->argc = rest_n;
    g->argv = rest;   /* caller frees via free(g->argv) */

    /* need at least entity + action */
    if (rest_n < 2) return EXIT_CLI;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  cmd_args_*  (pass 2)                                               */
/* ------------------------------------------------------------------ */

void cmd_args_init(cmd_args_t *it, int argc, char **argv) {
    it->argc = argc;
    it->argv = argv;
    it->pos  = 0;
}

/*
 * Internal: peek at current token.  Returns 0 if out of range.
 */
static int peek(cmd_args_t *it, const char **tok) {
    if (it->pos >= it->argc) { *tok = NULL; return 0; }
    *tok = it->argv[it->pos];
    return 1;
}

const char *cmd_args_next_positional(cmd_args_t *it) {
    while (it->pos < it->argc) {
        const char *tok = it->argv[it->pos];
        if (!is_flag(tok)) {
            it->pos++;
            return tok;
        }
        /* it's a flag; skip flag + optional value (heuristic:
         * next non-flag token is its value) */
        it->pos++;
        if (it->pos < it->argc) {
            const char *nxt = it->argv[it->pos];
            if (!is_flag(nxt)) it->pos++;
        }
    }
    return NULL;
}

const char *cmd_args_flag(cmd_args_t *it, const char *name, int has_value) {
    for (; it->pos < it->argc; it->pos++) {
        const char *tok = it->argv[it->pos];
        if (flag_prefix_match(tok, name)) {
            size_t nlen = strlen(name);
            char nc = tok[2 + nlen];
            if (nc == '=') {
                const char *val = tok + 2 + nlen + 1;
                it->pos++;
                return val;
            }
            it->pos++;
            if (has_value) {
                const char *v;
                if (peek(it, &v)) { it->pos++; return v; }
            }
            return NULL; /* boolean or missing value */
        }
    }
    return NULL;
}

int cmd_args_has_flag(cmd_args_t *it, const char *name) {
    for (int i = it->pos; i < it->argc; i++) {
        if (flag_prefix_match(it->argv[i], name)) return 1;
    }
    return 0;
}
