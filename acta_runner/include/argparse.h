#ifndef ACTA_RUNNER_ARGPARSE_H
#define ACTA_RUNNER_ARGPARSE_H

#include "runner.h"   /* global_opts_t (runner.h does NOT include this) */

/*
 * Hand-rolled argument parser (same two-pass design as the CLI).
 *
 * Pass 1 (parse_globals):  scan argv, pull out known global flags.
 *         Remaining args are compacted into g->argv / g->argc.
 *         The first remaining arg is the action.
 *
 * Pass 2 (cmd_args_*):     per-action iteration over the action's own
 *         positionals + flags.
 */

/*
 * Returns 0 on success, or EXIT_CLI if --version/--help was set
 * (caller should handle and exit).
 *
 * After the call, g->argv[0] = action, g->argv[1..] = positionals
 * + action flags.
 */
int parse_globals(int argc, char **argv, global_opts_t *g);

typedef struct {
    int      argc;
    char   **argv;
    int      pos;       /* current scan index */
} cmd_args_t;

void cmd_args_init(cmd_args_t *it, int argc, char **argv);

/*
 * Strict check of the pass-2 flags: every --name token must be a known
 * action flag; unknown long options are rejected (the CLI's old
 * behaviour silently ignored typos and produced wrong results).
 * Prints the JSON error to stderr. Returns EXIT_OK or EXIT_INVALID.
 */
int cmd_args_validate(const cmd_args_t *it);

/*
 * Returns the next positional (non-flag) argument, or NULL.
 * Skips past flags automatically; a flag's value token is skipped
 * only if the flag name is known to take a value — boolean flags
 * never eat the next token.
 */
const char *cmd_args_next_positional(cmd_args_t *it);

/*
 * Checks whether the flag --<name> is present (scans the whole arg
 * list). If present with a value, returns the value string (inline
 * --name=value or --name <value>); for boolean flags returns NULL
 * only when the flag is absent — callers that need the boolean use
 * cmd_args_has_flag.
 *
 * Usage:
 *   const char *v = cmd_args_flag(&it, "timeout", 1);
 *   if (!v) { // not present
 */
const char *cmd_args_flag(cmd_args_t *it, const char *name, int has_value);

/* Peeks: is --name present? Does NOT advance. */
int cmd_args_has_flag(const cmd_args_t *it, const char *name);

#endif /* ACTA_RUNNER_ARGPARSE_H */
