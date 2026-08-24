/*
 * actagamma_db — entry point
 *
 * Flow:
 *   1. parse_globals  → extract --db, --version, --help, --tools, etc.
 *   2. early-exit     → version / help / tools
 *   3. resolve DB path
 *   4. dispatch       → commands_dispatch(entity, action, args, opts)
 *   5. exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#include "cli.h"
#include "argparse.h"
#include "commands.h"

/*
 * Resolve DB path per spec §3:
 *   1. --db flag
 *   2. $ACTA_DB
 *   3. ./acta.db
 */
const char *resolve_db_path(const char *flag_db) {
    if (flag_db && flag_db[0]) return flag_db;
    const char *env = getenv("ACTA_DB");
    if (env && env[0]) return env;
    return "./acta.db";
}

/*
 * Single-line JSON error → stderr, empty stdout.
 */
void cli_error(int exit_code, const char *err_const, int c_code,
               const char *fmt, ...) {
    (void)exit_code;
    fprintf(stderr, "{\"error\":\"");
    if (err_const) fputs(err_const, stderr);
    fprintf(stderr, "\",\"code\":%d,\"message\":\"", c_code);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputs("\"}\n", stderr);
}

int main(int argc, char **argv) {
    global_opts_t gopts;
    memset(&gopts, 0, sizeof(gopts));

    /* ---- pass 1: global flags ---- */
    int rc = parse_globals(argc, argv, &gopts);
    if (rc == EXIT_CLI) {
        /* usage error (e.g. missing entity) */
        cli_error(EXIT_CLI, "ACTA_CLI_ERR", -10,
                  "missing entity and/or action. See --help.");
        return EXIT_CLI;
    }

    /* ---- early exits ---- */
    if (gopts.show_version) { version_print(stdout);  return EXIT_OK; }
    if (gopts.show_help)    { help_print(stdout);     return EXIT_OK; }
    if (gopts.show_tools)   { tools_print(stdout);    return EXIT_OK; }

    /* ---- need at least entity + action ---- */
    if (gopts.argc < 2) {
        cli_error(EXIT_CLI, "ACTA_CLI_ERR", -10,
                  "usage: actagamma_db <entity> <action> [args]. See --help.");
        return EXIT_CLI;
    }

    const char *entity = gopts.argv[0];
    const char *action = gopts.argv[1];

    /*
     * Positional args + entity flags start at gopts.argv[2].
     * For POC we pass them as a sub-argv to the handler.
     */
    cmd_args_t ga;
    cmd_args_init(&ga, gopts.argc - 2, gopts.argv + 2);

    /* ---- resolve DB (handlers will use gopts.db / env / default) ---- */
    const char *db = resolve_db_path(gopts.db);
    (void)db; /* TODO: open DB here or pass path to handlers */

    /* ---- dispatch ---- */
    rc = commands_dispatch(entity, action, &ga, &gopts);

    free(gopts.argv);   /* parse_globals allocated this */
    return rc;
}
