/*
 * acta_cli — entry point
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

#include <io.h>        /* dup, dup2, fileno, close — --out fd redirect */
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
 * (cli_util.h) — error name from the raw rc, `code` = exit code
 * negated (T2 invariant, |code| == exit), message formatted then
 * JSON-escaped (paths with `"`/`\` are safe). The exit code is
 * derived from rc (map_rc_to_exit) and returned.
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

    /* ---- pass 1: global flags ----
     * T2: parse_globals distinguishes OOM (EXIT_ALLOC), missing flag
     * value and too-few-positionals (EXIT_CLI) and emits the JSON error
     * line itself — just return its code here. */
    int rc = parse_globals(argc, argv, &gopts);
    if (rc != EXIT_OK)
        return rc;

    /* ---- early exits (no DB needed) ---- */
    if (gopts.show_version) { version_print(stdout);  return EXIT_OK; }
    if (gopts.show_help) {
        if (gopts.argc == 0) {
            /* bare `--help`: the global usage */
            help_print(stdout);
            free(gopts.argv);
            return EXIT_OK;
        }
        /* P0: scoped help.
         *   `entity --help`          → whole entity help
         *   `entity action --help`   → single-action section
         * Unknown entity or action → canonical exit-10 JSON error. */
        const char *action = (gopts.argc >= 2) ? gopts.argv[1] : NULL;
        int rc = entity_help(gopts.argv[0], action, stdout);
        free(gopts.argv);
        return rc;
    }
    if (gopts.show_tools)   { if (gopts.compact)
                                 tools_print_compact(stdout);
                              else
                                 tools_print(stdout, gopts.pretty);
                              return EXIT_OK; }

    /* ---- need at least entity + action ---- */
    if (gopts.argc < 2) {
        return cli_error(ACTA_DB_ERR_INVALID,
                         "usage: acta_cli <entity> <action> [args]. See --help.");
    }

    const char *entity = gopts.argv[0];
    const char *action = gopts.argv[1];

    /* Normalise documented flag aliases before dispatch
     * (--deleted → --include_deleted). */
    apply_flag_aliases(gopts.argv, gopts.argc);
    cmd_args_t ga;
    cmd_args_init(&ga, gopts.argc - 2, gopts.argv + 2);

    /* Unknown --name tokens must not be silently ignored: the old
     * behaviour let a typo'd filter flag through, which exited 0 with
     * a plausible-looking but wrong result set.  T2: unknown option
     * is a CLI-usage error — cmd_args_validate emits the JSON line
     * (ACTA_CLI_ERR, code -10) and returns EXIT_CLI. */
    int vrc = cmd_args_validate(&ga);
    if (vrc != EXIT_OK) {
        free(gopts.argv);
        return vrc;
    }

    /* P3: --raw_out is only meaningful for 'context get' / 'exec get'.
     * Any other use is a CLI usage error (fail fast, not silent). */
    if (gopts.raw_out &&
        !(((strcmp(entity, "context") == 0) ||
           (strcmp(entity, "exec") == 0)) &&
          strcmp(action, "get") == 0)) {
        free(gopts.argv);
        return emit_cli_error(
            "--raw_out only supported by 'context get' and 'exec get'");
    }

    /* P3: --out <path> — write the whole stdout payload to the file.
     * Implemented by redirecting fd 1 around the dispatch; stderr
     * (errors, warnings, VLOG) is untouched. */
    FILE *payload_out = NULL;
    int saved_stdout = -1;
    if (gopts.out_path) {
        payload_out = fopen(gopts.out_path, "w");
        if (!payload_out) {
            char msg[256];
            snprintf(msg, sizeof msg,
                     "cannot open output file '%s'", gopts.out_path);
            free(gopts.argv);
            return emit_cli_error(msg);
        }
        saved_stdout = dup(STDOUT_FILENO);
        if (dup2(fileno(payload_out), STDOUT_FILENO) < 0) {
            char msg[256];
            snprintf(msg, sizeof msg,
                     "cannot redirect stdout to '%s'", gopts.out_path);
            fclose(payload_out);
            free(gopts.argv);
            return emit_cli_error(msg);
        }
    }

    /* ---- resolve DB path ---- */
    const char *db_path = resolve_db_path(gopts.db);

    /* ---- open database ----
     * T2: a failed open is its own class — exit EXIT_DB_OPEN (11),
     * code -11, error name keeps the raw rc's granularity. */
    int db_err = ACTA_DB_OK;
    db_t *db = acta_db_open(db_path, &db_err, ACTA_DB_OPEN_EXISTING);
    if (!db) {
        char msg[2048];
        snprintf(msg, sizeof msg,
                 "cannot open database '%s' (%s): check the path and that "
                 "it is a valid SQLite database",
                 db_path, acta_db_strerror(db_err));
        free(gopts.argv);
        return emit_db_open_error(db_err, msg);
    }

    /* Informational pragma note after a successful open (e.g. WAL not
     * supported by the backend). Not an error. */
    if (const char *note = acta_db_last_error(db))
        fprintf(stderr, "[warn] db opened with degraded pragma state: %s\n",
                note);

    /* ---- dispatch (handlers receive the open db handle) ---- */
    rc = commands_dispatch(entity, action, &ga, &gopts, db);

    /* P3: restore stdout before anything else prints (db close warn). */
    if (payload_out) {
        fflush(stdout);
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        fclose(payload_out);
    }

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
