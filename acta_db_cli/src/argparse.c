#include "argparse.h"
#include "cli_util.h"   /* parse_nonneg_int, json_str */
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
        /* ---- --no_nulls ---- */
        if (strcmp(a, "--no_nulls") == 0) { g->no_nulls = 1; continue; }
        /* ---- --id_only ---- */
        if (strcmp(a, "--id_only") == 0)  { g->id_only = 1;  continue; }
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
            int lvl = 0;
            /* Only --verbose=N reaches here (bare --verbose is caught by the
             * strcmp above). Never consume argv[i+1]: a space form would
             * silently eat the entity name. */
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
                fprintf(stderr, "--verbose: level clamped to 3 (got %d)\n", lvl);
                g->verbose = 3;
            } else {
                g->verbose = lvl;
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
        /* ---- --from_file <path> ---- */
        if (flag_prefix_match(a, "from_file")) {
            if (a[11] == '=') g->from_file = a + 12;
            else {
                if (i + 1 >= argc) { free(rest); return EXIT_CLI; }
                g->from_file = argv[++i];
            }
            continue;
        }
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

/*
 * name → has_value table for ENTITY flags (global flags are already
 * consumed in pass 1).  The flag vocabulary is consistent across
 * entities: the same name always takes a value or is always boolean.
 * A flag not listed here is conservatively treated as value-taking
 * (legacy behavior), so new value flags keep working before they
 * are added to the table.
 */
typedef struct { const char *name; int has_value; } flag_spec_t;

static const flag_spec_t entity_flag_specs[] = {
    { "all", 0 },
    { "backend", 1 },
    { "base_url", 1 },
    { "configuration", 1 },
    { "content", 1 },
    { "context_id", 1 },
    { "count", 0 },
    { "description", 1 },
    { "error", 0 },
    { "event", 1 },
    { "execution_id", 1 },
    { "file", 1 },
    { "folder_id", 1 },
    { "hash", 1 },
    { "include_deleted", 0 },
    { "level", 1 },
    { "limit", 1 },
    { "live", 0 },
    { "message", 1 },
    { "metadata", 1 },
    { "model_identifier", 1 },
    { "model_revision_id", 1 },
    { "name", 1 },
    { "offset", 1 },
    { "output_schema", 1 },
    { "parent_execution_id", 1 },
    { "parent_id", 1 },
    { "prompt", 1 },
    { "prompt_template", 1 },
    { "raw", 1 },
    { "result", 0 },
    { "skill_revision_id", 1 },
    { "sql", 1 },
    { "sql_stdin", 0 },
    { "status", 1 },
    { "type", 1 },
};

static int flag_has_value(const char *name) {
    for (size_t i = 0; i < sizeof(entity_flag_specs) / sizeof(entity_flag_specs[0]); i++) {
        if (strcmp(entity_flag_specs[i].name, name) == 0)
            return entity_flag_specs[i].has_value;
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
         * takes one (name→has_value table).  The old positional
         * heuristic unconditionally ate the next non-flag token, so
         * boolean flags (e.g. --include_deleted) consumed the
         * positional that followed them. */
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


int cmd_args_has_flag(cmd_args_t *it, const char *name) {
    for (int i = it->pos; i < it->argc; i++) {
        if (flag_prefix_match(it->argv[i], name)) return 1;
    }
    return 0;
}
