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

/* Value part of an inline "--name=value" token: pointer just past the
 * '=', or NULL if the token is not in inline form. The offset is
 * derived from strlen(name) (2 for "--" + name length), so no magic
 * number can go stale if a flag is renamed. Only call after
 * flag_prefix_match(token, name) has succeeded. */
static const char *flag_inline_value(const char *token, const char *name) {
    const char *p = token + 2 + strlen(name);
    return *p == '=' ? p + 1 : NULL;
}

/* ------------------------------------------------------------------ */
/*  parse_globals                                                      */
/* ------------------------------------------------------------------ */

int parse_globals(int argc, char **argv, global_opts_t *g) {
    memset(g, 0, sizeof(*g));

    /* temp buffer for non-global args (OOM → EXIT_ALLOC, T2) */
    char **rest = malloc(sizeof(char *) * (size_t)argc);
    if (!rest)
        return finish_db_error(ACTA_DB_ERR_ALLOC,
                               "out of memory while parsing arguments");
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
        /* ---- --help ----
         * Do NOT return here (P0): the entity/action tokens still have
         * to be collected, so `model list --help` can route to the
         * single-action help. main() sees show_help + the collected
         * argv and picks global / entity / action help accordingly. */
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            g->show_help = 1;
            continue;
        }
        /* ---- --tools ----
         * Do NOT return here: a global flag placed after --tools
         * (e.g. `--tools --pretty`) must still be scanned, so keep the
         * pass going and short-circuit at the end via show_tools. */
        if (strcmp(a, "--tools") == 0) {
            g->show_tools = 1;
            continue;
        }
        /* ---- --db <path> ---- */
        if (flag_prefix_match(a, "db")) {
            const char *v = flag_inline_value(a, "db");
            if (v) g->db = v;
            else {
                if (i + 1 >= argc) {
                    free(rest);
                    return emit_cli_error("missing value for --db");
                }
                g->db = argv[++i];
            }
            continue;
        }
        /* ---- --fields <f1,f2> ---- */
        if (flag_prefix_match(a, "fields")) {
            const char *v = flag_inline_value(a, "fields");
            if (v) g->fields = v;
            else {
                if (i + 1 >= argc) {
                    free(rest);
                    return emit_cli_error("missing value for --fields");
                }
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
        /* ---- --stream ---- */
        if (strcmp(a, "--stream") == 0)   { g->stream = 1;   continue; }
        /* ---- --pretty ---- */
        if (strcmp(a, "--pretty") == 0)   { g->pretty = 1;   continue; }
        /* ---- --compact (meaningful with --tools) ---- */
        if (strcmp(a, "--compact") == 0)  { g->compact = 1;  continue; }
        /* ---- --verbose / -v (stackable, 0–3) ---- */
        if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            if (g->verbose < 3) g->verbose++;
            continue;
        }
        if (flag_prefix_match(a, "verbose")) {
            int lvl = 0;
            const char *v = flag_inline_value(a, "verbose");
            /* Only --verbose=N reaches here (bare --verbose is caught by the
             * strcmp above). Never consume argv[i+1]: a space form would
             * silently eat the entity name. */
            if (!v) {
                if (g->verbose < 3) g->verbose++;
                continue;
            }
            if (*v == '\0' || !parse_nonneg_int(v, &lvl)) {
                char msg[128];
                snprintf(msg, sizeof msg,
                         "invalid --verbose level: '%s' "
                         "(expected an integer 0-3)", v);
                free(rest);
                return emit_cli_error(msg);
            }
            if (lvl > 3) {
                /* T2: plain-text clamp line was outside the error-line
                 * contract; demote to a VLOG-only diagnostic. */
                VLOG(3, "--verbose: level clamped to 3 (got %d)", lvl);
                g->verbose = 3;
            } else {
                g->verbose = lvl;
            }
            continue;
        }
        /* ---- --json <blob> ---- */
        if (flag_prefix_match(a, "json")) {
            const char *v = flag_inline_value(a, "json");
            if (v) g->json_input = v;
            else {
                if (i + 1 >= argc) {
                    free(rest);
                    return emit_cli_error("missing value for --json");
                }
                g->json_input = argv[++i];
            }
            continue;
        }
        /* ---- --stdin ---- */
        if (strcmp(a, "--stdin") == 0)    { g->from_stdin = 1;  continue; }
        /* ---- --from_file <path> ---- */
        if (flag_prefix_match(a, "from_file")) {
            const char *v = flag_inline_value(a, "from_file");
            if (v) g->from_file = v;
            else {
                if (i + 1 >= argc) {
                    free(rest);
                    return emit_cli_error("missing value for --from_file");
                }
                g->from_file = argv[++i];
            }
            continue;
        }
        /* ---- --out <path> (P3: payload → file instead of stdout) ---- */
        if (flag_prefix_match(a, "out")) {
            const char *v = flag_inline_value(a, "out");
            if (v) g->out_path = v;
            else {
                if (i + 1 >= argc) {
                    free(rest);
                    return emit_cli_error("missing value for --out");
                }
                g->out_path = argv[++i];
            }
            continue;
        }
        /* ---- --raw_out <field> (P3: raw single-field output,
         * context get / exec get) ---- */
        if (flag_prefix_match(a, "raw_out")) {
            const char *v = flag_inline_value(a, "raw_out");
            if (v) g->raw_out = v;
            else {
                if (i + 1 >= argc) {
                    free(rest);
                    return emit_cli_error("missing value for --raw_out");
                }
                g->raw_out = argv[++i];
            }
            continue;
        }
        /* not a recognised global → keep as entity/action/positional */
        rest[rest_n++] = a;
    }

    /* --tools short-circuits: no entity/action needed, rest (if any)
     * is ignored (main() returns before dispatch). */
    if (g->show_tools) {
        free(rest);
        g->argc = 0;
        g->argv = NULL;
        return 0;
    }

    g->argc = rest_n;
    g->argv = rest;   /* caller frees via free(g->argv) */

    /* need at least entity + action (T2: JSON error line emitted here,
     * so main() can just return the code).  --help is exempt: the P0
     * help router accepts bare `--help`, `entity --help`, and
     * `entity action --help`. */
    if (rest_n < 2 && !g->show_help)
        return emit_cli_error("missing entity and/or action. See --help.");
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
    { "content_file", 1 },
    { "context_id", 1 },
    { "count", 0 },
    { "deleted", 0 },
    { "description", 1 },
    { "error", 1 },
    { "event", 1 },
    { "execution_id", 1 },
    { "file", 1 },
    { "full", 0 },
    { "folder_id", 1 },
    { "hash", 1 },
    { "include_deleted", 0 },
    { "level", 1 },
    { "limit", 1 },
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
    { "raw_file", 1 },
    { "result", 1 },
    { "result_file", 1 },
    { "skill_revision_id", 1 },
    { "sql", 1 },
    { "sql_stdin", 0 },
    { "status", 1 },
    { "stream", 0 },
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

int cmd_args_count_positionals(const cmd_args_t *it) {
    int n = 0, p = it->pos;
    while (p < it->argc) {
        const char *tok = it->argv[p];
        if (!is_flag(tok)) { n++; p++; continue; }
        p++;
        if (strchr(tok + 2, '=')) continue;   /* --name=value: inline */
        if (flag_has_value(tok + 2) &&
            p < it->argc && !is_flag(it->argv[p]))
            p++;
    }
    return n;
}

const char *cmd_args_kth_positional(const cmd_args_t *it, int k) {
    int n = 0, p = it->pos;
    while (p < it->argc) {
        const char *tok = it->argv[p];
        if (!is_flag(tok)) {
            if (n == k) return tok;
            n++; p++; continue;
        }
        p++;
        if (strchr(tok + 2, '=')) continue;   /* --name=value: inline */
        if (flag_has_value(tok + 2) &&
            p < it->argc && !is_flag(it->argv[p]))
            p++;
    }
    return NULL;
}

/*
 * Rewrite documented flag aliases to their canonical names, in place
 * on the raw argv pointer array (g->argv), BEFORE cmd_args_init().
 * Currently: --deleted → --include_deleted (boolean). The rewrite lets
 * every entity handler (get / list / count) honour the alias through
 * its existing cmd_args_has_flag(..., "include_deleted") call.
 */
void apply_flag_aliases(char **argv, int argc) {
    for (int i = 0; i < argc; i++) {
        if (flag_prefix_match(argv[i], "deleted"))
            argv[i] = (char *)"--include_deleted";
    }
}

/*
 * Strict pass over the pass-2 flags: every --name token must be a known
 * entity flag (entity_flag_specs).  Before this check, unknown long
 * options were silently ignored, so a typo such as `model list
 * --deletd` exited 0 while quietly dropping soft-deleted rows.
 * Returns EXIT_OK, or emits the JSON CLI-usage error to stderr
 * (ACTA_CLI_ERR, code -10) and returns EXIT_CLI (T2).
 */
int cmd_args_validate(const cmd_args_t *it) {
    for (int i = 0; i < it->argc; i++) {
        const char *tok = it->argv[i];
        if (!is_flag(tok)) continue;
        const char *eq = strchr(tok + 2, '=');
        size_t len = eq ? (size_t)(eq - (tok + 2)) : strlen(tok + 2);
        int known = 0;
        for (size_t k = 0;
             k < sizeof(entity_flag_specs) / sizeof(entity_flag_specs[0]); k++) {
            if (strlen(entity_flag_specs[k].name) == len &&
                strncmp(entity_flag_specs[k].name, tok + 2, len) == 0) {
                known = 1;
                break;
            }
        }
        if (!known) {
            char msg[128];
            snprintf(msg, sizeof msg, "unknown option '%s' (see --help)",
                     tok);
            return emit_cli_error(msg);
        }
        /* A value-taking flag in the space form must be followed by a
         * non-flag value token; `--name --table` or `--name` as the last
         * token is a CLI usage error (exit 10), mirroring pass 1's
         * "missing value for --db" (T2). The inline form `--name=`
         * carries its value in the token itself (possibly empty). */
        if (flag_has_value(tok + 2) && eq == NULL) {
            const char *next = (i + 1 < it->argc) ? it->argv[i + 1] : NULL;
            if (next == NULL || is_flag(next)) {
                char msg[128];
                snprintf(msg, sizeof msg, "missing value for %s", tok);
                return emit_cli_error(msg);
            }
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

        /* Space form: the value is the next token ONLY if it is not
         * itself a flag — the same tokenization the positional walkers
         * use. `--name --table` therefore yields NULL here (and
         * cmd_args_validate reports it as a missing-value CLI usage
         * error); before this guard the flag token was silently
         * returned as the value (known issue 9). */
        if (has_value && i + 1 < it->argc &&
            !is_flag(it->argv[i + 1])) {
            return it->argv[i + 1];
        }
        return NULL;
    }
    return NULL;
}


int cmd_args_has_flag(cmd_args_t *it, const char *name) {
    /* Same protocol as cmd_args_flag: full range, no pos advance. */
    for (int i = 0; i < it->argc; i++) {
        if (flag_prefix_match(it->argv[i], name)) return 1;
    }
    return 0;
}
