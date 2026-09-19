#ifndef ACTA_DB_CLI_ARGPARSE_H
#define ACTA_DB_CLI_ARGPARSE_H

#include "cli.h"

/*
 * Hand-rolled argument parser.
 *
 * Pass 1 (parse_globals):  scan argv, pull out known global flags.
 *         Remaining args are compacted into g->argv / g->argc.
 *         The first two remaining args are entity and action.
 *
 * Pass 2 (cmd_arg_*):     per-command iteration over the handler's
 *         own positionals + flags.  The handler knows its own flag
 *         set; these helpers just do the mechanical scanning.
 */

/* ---- pass 1: global extraction ---- */
/*
 * Returns EXIT_OK (0) on success.  --version / --help / --tools set the
 * corresponding g->show_* flag and still return 0; the caller (main.c)
 * checks those flags and exits early.  --version short-circuits the
 * scan; --help and --tools keep collecting the entity/action tokens
 * (help routing needs them; --tools ignores them).  On failure the
 * single-line JSON error is already on stderr and the function returns:
 *   - EXIT_ALLOC  OOM building the rest buffer
 *   - EXIT_CLI    missing value for a value-taking global flag, or
 *                 fewer than two positionals (entity + action)
 *
 * After a successful call, g->argv[0] = entity, g->argv[1] = action,
 * g->argv[2..] = positionals + entity flags.  g->argv is owned by the
 * caller (free it with free(g->argv)).
 */
int parse_globals(int argc, char **argv, global_opts_t *g);

/* ---- pass 2: per-command iteration ---- */
typedef struct {
    int      argc;
    char   **argv;
    int      pos;       /* current scan index */
} cmd_args_t;

void cmd_args_init(cmd_args_t *it, int argc, char **argv);

/*
 * Rewrite documented flag aliases to canonical names, in place, on the
 * raw argv pointer array. Call BEFORE cmd_args_init().
 * Currently: --deleted → --include_deleted.
 */
void apply_flag_aliases(char **argv, int argc);

/*
 * Strict check of the pass-2 flags: every --name token must be a known
 * entity flag; unknown long options used to be silently ignored (a
 * typo'd filter flag then produced a plausible-looking but wrong list).
 * A value-taking flag in the space form must also be followed by a
 * non-flag value token — `--name --table` or `--name` as the last token
 * is rejected ("missing value for --name") instead of silently losing
 * the filter (known issue 9; mirrors pass 1's missing-value errors).
 * Prints the JSON CLI-usage error to stderr. Returns EXIT_OK, or
 * EXIT_CLI on an unknown option or a missing flag value (T2 error
 * contract).
 */
int cmd_args_validate(const cmd_args_t *it);

/*
 * Returns the next positional (non-flag) argument, or NULL.
 * Skips past flags automatically; a flag's value token is skipped
 * only if the flag name is known to take a value (name→has_value
 * table in argparse.c) — boolean flags never eat the next token.
 */
const char *cmd_args_next_positional(cmd_args_t *it);

/*
 * Read-only positional scans: same tokenization as
 * cmd_args_next_positional (value-taking flags eat their value), but
 * they never advance it->pos.  Used to detect surplus positionals
 * BEFORE a handler runs.
 */
int cmd_args_count_positionals(const cmd_args_t *it);
const char *cmd_args_kth_positional(const cmd_args_t *it, int k);

/*
 * Flag accessor.  One protocol, no state mutation:
 *   - scans the FULL argument range (argv[0..argc), never advances pos;
 *   - returns the value string when the flag is present and takes a
 *     value (--name <v> or --name=<v>);
 *   - returns NULL both when the flag is absent AND when it is present
 *     as a boolean — callers cannot distinguish those two; use
 *     cmd_args_has_flag for boolean flags.
 *
 * Usage:
 *   const char *v = cmd_args_flag(&it, "name", 1);
 *   if (!v) { // --name absent (or given without a value) }
 *   int has = cmd_args_has_flag(&it, "no_nulls");
 */
const char *cmd_args_flag(cmd_args_t *it, const char *name, int has_value);

/*
 * Presence check for --<name>.  Scans the full argument range; does
 * NOT advance pos.  Use this, not cmd_args_flag(..., 0), for boolean
 * flags — cmd_args_flag returns NULL both for "absent" and for
 * "boolean present".
 */
int cmd_args_has_flag(cmd_args_t *it, const char *name);


#endif /* ACTA_ARGPARSE_H */
