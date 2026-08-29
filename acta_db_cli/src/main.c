/*
 * actagamma_db — entry point
 *
 * Flow:
 *   1. parse_globals  → extract --db, --version, --help, --tools, etc.
 *   2. early-exit     → version / help / tools
 *   3. resolve DB path
 *   4. open DB
 *   5. dispatch       → commands_dispatch(entity, action, args, opts, db)
 *   6. close DB
 *   7. exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#include "cli.h"        /* global_opts_t, cmd_args_t, EXIT_* codes */
#include "cli_util.h"   /* json_str, map_rc_to_exit, finish_db_error */
#include "argparse.h"
#include "commands.h"
#include "acta_db.h"

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
 * Enforces the §7.1 schema once: delegates to finish_db_error
 * (cli_util.h) — error name from the raw rc, rc in "code",
 * message formatted then JSON-escaped (paths with `"`/`\` are
 * safe). The exit code is derived from rc (map_rc_to_exit) and
 * returned, so it always matches the "code" field.
 */
int cli_error(int rc, const char *fmt, ...) {
    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    if (vsnprintf(msg, sizeof msg, fmt, ap) < 0)
        msg[0] = '\0';
    va_end(ap);
    return finish_db_error(rc, msg);
}

int main(int argc, char **argv) {
    global_opts_t gopts;
    memset(&gopts, 0, sizeof(gopts));

    /* ---- pass 1: global flags ---- */
    int rc = parse_globals(argc, argv, &gopts);
    if (rc == EXIT_CLI) {
        return cli_error(ACTA_DB_ERR_INVALID,
                         "missing entity and/or action. See --help.");
    }

    /* ---- early exits (no DB needed) ---- */
    if (gopts.show_version) { version_print(stdout);  return EXIT_OK; }
    if (gopts.show_help)    { help_print(stdout);     return EXIT_OK; }
    if (gopts.show_tools)   { tools_print(stdout);    return EXIT_OK; }

    /* ---- need at least entity + action ---- */
    if (gopts.argc < 2) {
        return cli_error(ACTA_DB_ERR_INVALID,
                         "usage: actagamma_db <entity> <action> [args]. See --help.");
    }

    const char *entity = gopts.argv[0];
    const char *action = gopts.argv[1];

    cmd_args_t ga;
    cmd_args_init(&ga, gopts.argc - 2, gopts.argv + 2);

    /* ---- resolve DB path ---- */
    const char *db_path = resolve_db_path(gopts.db);

    /* ---- open database ---- */
    int db_err = ACTA_DB_OK;
    db_t *db = acta_db_open(db_path, &db_err, ACTA_DB_OPEN_EXISTING);
    if (!db) {
        /* Raw library rc flows into the JSON "code" field; the exit
         * code is map_rc_to_exit(db_err) (see #5: unknown rc maps
         * to EXIT_SQL until map_rc_to_exit is completed). */
        int open_exit = cli_error(db_err,
                                 "cannot open database '%s' (%s)",
                                 db_path, acta_db_strerror(db_err));
        free(gopts.argv);
        return open_exit;
    }

    /* ---- dispatch (handlers receive the open db handle) ---- */
    rc = commands_dispatch(entity, action, &ga, &gopts, db);

    /* ---- close database ---- */
    int close_rc = acta_db_close(db);
    if (close_rc != ACTA_DB_OK) {
        fprintf(stderr, "[warn] db close returned %s\n",
                acta_db_strerror(close_rc));
        acta_db_force_close(db);
    }

    free(gopts.argv);
    return rc;
}
