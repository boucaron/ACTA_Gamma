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
 * Returns 0 on success, or EXIT_CLI if --version/--help/--tools
 * was set (caller should handle and exit).
 *
 * After the call, g->argv[0] = entity, g->argv[1] = action,
 * g->argv[2..] = positionals + entity flags.
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
 * Prints the JSON error to stderr. Returns EXIT_OK or EXIT_INVALID.
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
 * Checks whether the flag --<name> is present.
 * If present, advances past it (and its value if <has_value>).
 * Returns the value string (or NULL for boolean flags), or
 * NULL if the flag was not found.
 *
 * Usage:
 *   const char *v = cmd_args_flag(&it, "name", 1);
 *   if (!v) { // not present  
 *   // v is the value for --name <v>
 *   // v is NULL for --no_nulls (boolean)
 */
const char *cmd_args_flag(cmd_args_t *it, const char *name, int has_value);

/*
 * Peeks ahead: is --name present? Does NOT advance.
 */
int cmd_args_has_flag(cmd_args_t *it, const char *name);


#endif /* ACTA_ARGPARSE_H */
