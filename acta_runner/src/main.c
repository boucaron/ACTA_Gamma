/*
 * acta_runner — entry point (Plan A: standalone LLM execution runner)
 *
 * Drives pending executions from the database through the acta_db
 * execution lifecycle against the model's OpenAI-compatible backend.
 *
 * Flow (mirrors acta_cli/main.c):
 *   1. parse_globals  → --db, --version, --help, --verbose
 *   2. early-exit     → version / help
 *   3. require action → "run" / "check" / "sweep"
 *   4. resolve DB path
 *   5. open DB
 *   6. dispatch       → commands_dispatch(action, args, opts, db)
 *   7. close DB
 *   8. exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "runner.h"
#include "runner_util.h"
#include "argparse.h"
#include "acta_db.h"
#include "acta_dbpath.h"  /* DB path resolution shared with acta_cli */
#include "deathmark.h"

/*
 * Single-line JSON error → stderr, empty stdout. Enforces the error
 * contract once (same shape as finish_db_error in runner_util.h).
 * The exit code is derived from rc (map_rc_to_exit) and returned, so
 * it always matches the "code" field.
 */
int runner_error(int rc, const char *fmt, ...)
{
    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    if (vsnprintf(msg, sizeof msg, fmt, ap) < 0)
        msg[0] = '\0';
    va_end(ap);
    return finish_db_error(rc, msg);
}

static void version_print(FILE *out)
{
    fprintf(out, "acta_runner %s (git %s)\n",
            ACTA_RUNNER_VERSION, ACTA_RUNNER_GIT_HASH);
}

static void help_print(FILE *out)
{
    fputs(
        "acta_runner " ACTA_RUNNER_VERSION
        " — LLM execution runner (drives pending executions against the\n"
        "model's OpenAI-compatible backend, logging to the DB)\n"
        "\n"
        "Usage:\n"
        "  acta_runner <action> [args] [global options]\n"
        "\n"
        "Actions:\n"
        "  run <execution-id>    Run one pending execution\n"
        "  run --pending         Run pending executions (up to --max)\n"
        "  check <model-record-id>  Verify the backend and model WITHOUT\n"
        "                          any chat completion (zero tokens, no\n"
        "                          execution rows, no DB writes)\n"
        "  check --base-url <url> --model-identifier <id>\n"
        "                          (standalone: no DB lookup)\n"
        "  sweep                 Fail stale `running` executions\n"
        "\n"
        "run flags:\n"
        "  --pending             Run pending executions instead of one id\n"
        "  --max <n>             Max executions to run with --pending (0 = no limit)\n"
        "  --timeout <sec>       Backend timeout in seconds (default:\n"
        "                        config file \"timeout\", else 600)\n"
        "\n"
        "check flags:\n"
        "  --base-url <url>          Standalone mode: probe this server\n"
        "  --model-identifier <id>   Standalone mode (with --base-url);\n"
        "                            mutually exclusive with the positional\n"
        "                            <model-record-id>\n"
        "  --timeout <sec>           Backend timeout in seconds (default:\n"
        "                        config file \"timeout\", else 600)\n"
        "\n"
        "sweep flags:\n"
        "  --stale-seconds <n>   Seconds of inactivity before a `running`\n"
        "                        row is stale (positive integer, required)\n"
        "\n"
        "Global options:\n"
        "  --db <path>           Database file (default: $ACTA_DB, else the\n"
        "                        config file's \"db\", else the platform\n"
        "                        app-data location — same file the GUI uses;\n"
        "                        ./acta.db as a last resort)\n"
        "  -v / --verbose        Stackable verbose level 0–3 (stderr)\n"
        "  --version             Print version and exit\n"
        "  --help                Print this help and exit\n"
        "\n"
        "Exit codes:\n"
        "  0 ok | 1 not found | 2 sql | 3 alloc | 4 invalid\n"
        "  10 cli error | 11 db open | 12 http | 13 timeout | "
        "14 cancelled (UI cancel only; never emitted by the CLI)\n",
        out);
}

int commands_dispatch(const char *action, cmd_args_t *args,
                      const global_opts_t *gopts, db_t *db)
{
    (void)gopts;

    if (strcmp(action, "help") == 0) {
        help_print(stdout);
        return EXIT_OK;
    }
    if (strcmp(action, "run") == 0) {
        return cmd_run(args, gopts, db);
    }
    if (strcmp(action, "check") == 0) {
        return cmd_check(args, gopts, db);
    }
    if (strcmp(action, "sweep") == 0) {
        return cmd_sweep(args, gopts, db);
    }

    /* Unknown action. */
    fprintf(stderr, "Unknown action '%s'.\n", action ? action : "(null)");
    fprintf(stderr, "  Run 'acta_runner --help' for full usage.\n");
    return EXIT_INVALID;
}

/* The single process-wide options pointer used by VLOG() and helpers
 * (same pattern as cli_gopts in the CLI). Set once in main(). */
const global_opts_t *runner_gopts;

int main(int argc, char **argv)
{
    global_opts_t gopts;
    memset(&gopts, 0, sizeof(gopts));

    /* ---- pass 1: global flags ---- */
    int rc = parse_globals(argc, argv, &gopts);
    if (rc == EXIT_CLI) {
        int e = runner_error(ACTA_DB_ERR_INVALID,
                             "missing action. See --help.");
        free(gopts.argv);
        return e;
    }

    /* ---- early exits (no DB needed) ---- */
    if (gopts.show_version) { version_print(stdout); return EXIT_OK; }
    if (gopts.show_help)    { help_print(stdout);     return EXIT_OK; }

    /* ---- need at least an action ---- */
    const char *action = gopts.argv[0];

    /* Remaining tokens are the action's positionals + flags. */
    cmd_args_t ga;
    cmd_args_init(&ga, gopts.argc - 1, gopts.argv + 1);

    /* Unknown --name tokens must not be silently ignored (the CLI's
     * old behaviour let a typo'd flag through and exited 0 with a
     * plausible-looking but wrong result). */
    if (cmd_args_validate(&ga) != EXIT_OK) {
        free(gopts.argv);
        return EXIT_INVALID;
    }

    /* ---- "check" standalone mode skips the DB entirely ----
     * `check --base-url <url> --model-identifier <id>` probes a server
     * before any model is registered, so there is no DB lookup: the
     * action is dispatched with a NULL db handle, which cmd_check must
     * not touch. (DB-mode `check <model-record-id>` needs the open
     * handle for the model-row read.) */
    if (strcmp(action, "check") == 0 &&
        cmd_args_has_flag(&ga, "base-url") &&
        cmd_args_has_flag(&ga, "model-identifier")) {
        runner_gopts = &gopts;
        int rc = commands_dispatch(action, &ga, &gopts, NULL);
        free(gopts.argv);
        return rc;
    }

    /* ---- resolve DB path ----
     * --db → $ACTA_DB → config file "db" → platform app-data default
     * (same file the GUI uses) → ./acta.db (last resort).
     * See acta_dbpath.h. A readable-but-malformed config file is a
     * hard error (fail-closed; docs/cli_spec.md, DB-file resolution). */
    char *conf_err = NULL;
    const char *db_path = acta_db_resolve_db_path(gopts.db, &conf_err);
    if (!db_path) {
        const char *what = conf_err ? conf_err : "config file is invalid";
        VLOG(1, "main: ERROR %s", what);
        free(conf_err);
        free(gopts.argv);
        return runner_error(ACTA_DB_ERR_INVALID, "%s", what);
    }

    /* ---- open database ---- */
    int db_err = ACTA_DB_OK;
    db_t *db = acta_db_open(db_path, &db_err, ACTA_DB_OPEN_EXISTING);
    if (!db) {
        char msg[2048];
        int len = snprintf(msg, sizeof msg,
                           "cannot open database '%s' (%s): check the path "
                           "and that it is a valid SQLite database",
                           db_path, acta_db_strerror(db_err));
        /* Backward-compat: the default path was used (no --db, no
         * $ACTA_DB, no config-file "db"), it cannot be opened, and a
         * legacy ./acta.db exists in the working directory → point the
         * user at it. */
        if (db_err == ACTA_DB_ERR_INVALID_DB &&
            strcmp(db_path, acta_db_default_db_path()) == 0) {
            char hint[256];
            if (acta_db_legacy_db_hint(hint, sizeof hint))
                snprintf(msg + len, sizeof msg - (size_t)len, " %s", hint);
        }
        int open_exit = runner_error(db_err, "%s", msg);
        free(gopts.argv);
        return open_exit;
    }

    /* Informational pragma note after a successful open (e.g. WAL not
     * supported by the backend). Not an error. */
    if (const char *note = acta_db_last_error(db))
        fprintf(stderr, "[warn] db opened with degraded pragma state: %s\n",
                note);

    /* Death-marker exit path: on a clean exit (SIGINT/SIGTERM/atexit)
     * while an execution is claimed in `running`, the row is marked
     * failed instead of left orphaned
     * (docs/plans/runner-ops-hardening.md, item 3). */
    deathmark_install();

    /* ---- dispatch (handler receives the open db handle) ---- */
    runner_gopts = &gopts;
    rc = commands_dispatch(action, &ga, &gopts, db);

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
