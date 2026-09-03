#include "argparse.h"
#include "runner_util.h"
#include <string.h>
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
        /* ---- --help / -h ---- */
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            free(rest);
            g->show_help = 1;
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
        /* ---- --verbose / -v (stackable, 0–3) ---- */
        if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            if (g->verbose < 3) g->verbose++;
            continue;
        }
        if (flag_prefix_match(a, "verbose")) {
            int lvl = 0;
            /* Only --verbose=N reaches here (bare --verbose is caught by
             * the strcmp above). Never consume argv[i+1]: a space form
             * would silently eat the action name. */
            if (a[9] != '=') {
                if (g->verbose < 3) g->verbose++;
                continue;
            }
            if (a[10] == '\0' || !parse_nonneg_int(a + 10, &lvl)) {
                fprintf(stderr,
                    "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
                    "\"message\":\"invalid --verbose level: '");
                json_str(stderr, a + 10);
                fprintf(stderr,
                    "' (expected an integer 0-3)\"}\n");
                free(rest);
                return EXIT_INVALID;
            }
            if (lvl > 3) {
                fprintf(stderr, "--verbose: level clamped to 3 (got %d)\n",
                        lvl);
                g->verbose = 3;
            } else {
                g->verbose = lvl;
            }
            continue;
        }
        /* not a recognised global → keep as action/positional/flag */
        rest[rest_n++] = a;
    }

    g->argc = rest_n;
    g->argv = rest;   /* caller frees via free(g->argv) */

    /* need at least an action */
    if (rest_n < 1) return EXIT_CLI;
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
 * name → has_value table for the RUNNER's action flags. The flag
 * vocabulary is stable: the same name always takes a value or is
 * always boolean.
 */
typedef struct { const char *name; int has_value; } flag_spec_t;

static const flag_spec_t runner_flag_specs[] = {
    { "pending", 0 },
    { "max",     1 },
    { "timeout", 1 },
    { "api_key", 1 },
};

static int flag_has_value(const char *name) {
    for (size_t i = 0;
         i < sizeof(runner_flag_specs) / sizeof(runner_flag_specs[0]); i++) {
        if (strcmp(runner_flag_specs[i].name, name) == 0)
            return runner_flag_specs[i].has_value;
    }
    return 1;   /* unknown flag: assume value-taking (legacy) */
}

const char *cmd_args_next_positional(cmd_args_t *it) {
    while (it->pos < it->argc) {
        const char *tok = it->argv[it->pos];
        if (!is_flag(tok)) {
            it->pos++;
            return tok;
        }
        /* it's a flag; skip it, and skip its value ONLY if the flag
         * takes one (name→has_value table). Boolean flags never eat
         * the positional that follows them. */
        it->pos++;
        if (strchr(tok + 2, '=')) continue;   /* --name=value: inline */
        if (flag_has_value(tok + 2) &&
            it->pos < it->argc &&
            !is_flag(it->argv[it->pos])) {
            it->pos++;
        }
    }
    return NULL;
}

/*
 * Strict pass over the pass-2 flags: every --name token must be a known
 * runner flag. Prints the JSON error to stderr and returns
 * EXIT_INVALID; otherwise returns EXIT_OK.
 */
int cmd_args_validate(const cmd_args_t *it) {
    for (int i = 0; i < it->argc; i++) {
        const char *tok = it->argv[i];
        if (!is_flag(tok)) continue;
        const char *eq = strchr(tok + 2, '=');
        size_t len = eq ? (size_t)(eq - (tok + 2)) : strlen(tok + 2);
        int known = 0;
        for (size_t k = 0;
             k < sizeof(runner_flag_specs) / sizeof(runner_flag_specs[0]);
             k++) {
            if (strlen(runner_flag_specs[k].name) == len &&
                strncmp(runner_flag_specs[k].name, tok + 2, len) == 0) {
                known = 1;
                break;
            }
        }
        if (!known) {
            fprintf(stderr,
                "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
                "\"message\":\"unknown option '");
            json_str(stderr, tok);
            fprintf(stderr, "' (see --help)\"}\n");
            return EXIT_INVALID;
        }
    }
    return EXIT_OK;
}

const char *cmd_args_flag(cmd_args_t *it, const char *name, int has_value)
{
    for (int i = 0; i < it->argc; i++) {
        const char *tok = it->argv[i];
        if (!flag_prefix_match(tok, name))
            continue;

        size_t nlen = strlen(name);
        char nc = tok[2 + nlen];

        if (nc == '=') {
            return tok + 2 + nlen + 1;
        }

        if (has_value && i + 1 < it->argc) {
            return it->argv[i + 1];
        }
        return NULL;
    }
    return NULL;
}

int cmd_args_has_flag(const cmd_args_t *it, const char *name) {
    for (int i = 0; i < it->argc; i++)
        if (flag_prefix_match(it->argv[i], name))
            return 1;
    return 0;
}
