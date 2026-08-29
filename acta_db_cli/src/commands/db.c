#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════ */
/*  Usage / help                                                       */
/* ══════════════════════════════════════════════════════════════════ */

/* Non-static: the global dispatch layer can call this for
 *   actagamma_db db --help                                                      */
void db_usage(FILE *f)
{
    fputs(
"Usage: actagamma_db db <action> [options]\n"
"\n"
"Actions:\n"
"  exec      Execute mutating SQL (INSERT, UPDATE, DELETE, DDL, etc.)\n"
"  version   Print SQLite library version\n"
"  help      Show this help\n"
"\n"
"== exec =========================================================\n"
"  Execute a single mutating statement (no SELECT / query support).\n"
"  Supported: INSERT, UPDATE, DELETE, CREATE, DROP, ALTER,\n"
"             TRUNCATE, REPLACE, and other write / DDL statements.\n"
"\n"
"  Provide SQL via one of (mutually exclusive, first wins):\n"
"\n"
"    actagamma_db db exec \"INSERT INTO users (name) VALUES ('Ada');\"\n"
"        <- positional argument\n"
"\n"
"    actagamma_db db exec --sql \"DELETE FROM sessions WHERE expires < now;\"\n"
"        <- --sql flag\n"
"\n"
"    actagamma_db db exec --file /path/to/migration.sql\n"
"        <- --file flag (max 64 KiB)\n"
"\n"
"    actagamma_db db exec --stdin\n"
"    cat migration.sql | actagamma_db db exec --stdin\n"
"        <- --stdin flag (max 64 KiB)\n"
"\n"
"  Options:\n"
"    --sql <text>       SQL text to execute\n"
"    --file <path>      Read SQL from a file (max 64 KiB)\n"
"    --stdin            Read SQL from stdin (max 64 KiB)\n"
"    --table            print 'ok' instead of JSON\n"
"    --verbose <n>      debug level 0-3 (stderr)\n"
"\n"
"== version ======================================================\n"
"  actagamma_db db version\n"
"  Prints the SQLite library version.\n"
"  Options:\n"
"    --table          print 'SQLite <ver>' instead of JSON\n"
"\n"
"Global options:\n"
"  --table          columnar / plain output instead of JSON\n"
"  --verbose <n>    debug level 0-3 (diagnostics on stderr)\n"
"\n", f);
}



/* ── helpers ───────────────────────────────────────────────────────── */

static void exec_output(const global_opts_t *gopts)
{
    if (gopts->table)
        fprintf(stdout, "ok\n");
    else
        fprintf(stdout, "{\"status\":\"ok\"}\n");
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t db_actions[] = {
    { "exec",    "execute mutating SQL (no SELECT)" },
    { "version", "print SQLite library version"     },
    { "help",   "show this help"                   },
};

#define DB_ACTIONS (sizeof(db_actions) / sizeof(db_actions[0]))

int cmd_db(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
           db_t *db)
{
    /* ── help (subcommand-level; only the bare word "help") ─────── */
    if (strcmp(action, "help") == 0) {
        db_usage(stdout);
        return EXIT_OK;
    }

    /* ── exec ─────────────────────────────────────────────────────── */
    if (strcmp(action, "exec") == 0) {
        const char *sql   = cmd_args_flag(ga, "sql",   1);
        const char *fpath = cmd_args_flag(ga, "file",  1);
        const int   use_stdin = (cmd_args_flag(ga, "stdin", 0) != NULL);

        VLOG(1, "db exec: sql=%s file=%s stdin=%d",
             sql    ? sql    : "(null)",
             fpath  ? fpath  : "(null)",
             use_stdin);

        VLOG(2, "  gopts: fields=%s no_nulls=%d table=%d id_only=%d verbose=%d",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table, gopts->verbose);

        if (!sql && !fpath && !use_stdin) {
            VLOG(1, "  ERROR: no SQL source (need positional, --sql, --file, or --stdin)");
            fprintf(stderr,
                "Error: no SQL source provided.\n"
                "  Provide SQL via one of:\n"
                "    actagamma_db db exec \"<SQL>\"            <- positional\n"
                "    actagamma_db db exec --sql \"<SQL>\"      <- --sql flag\n"
                "    actagamma_db db exec --file <path>      <- --file flag\n"
                "    actagamma_db db exec --stdin            <- stdin\n"
                "  Run 'actagamma_db db help' for full usage.\n");
            return EXIT_INVALID;
        }


        /* resolve SQL text */
        char   *sql_buf = NULL;
        const char *sql_ptr = NULL;

        if (sql) {
            sql_ptr = sql;
        } else if (fpath) {
            VLOG(2, "  reading file: %s", fpath);
            FILE *fp = fopen(fpath, "r");
            if (!fp) {
                VLOG(1, "  ERROR: cannot open file '%s'", fpath);
                fprintf(stderr,
                    "Error: cannot open file '%s'.\n"
                    "  Check the path and permissions.\n"
                    "  Usage: actagamma_db db exec --file <path>\n"
                    "  Run 'actagamma_db db help' for full usage.\n",
                    fpath);
                return EXIT_INVALID;
            }
            long sz = 0;
            fseek(fp, 0, SEEK_END);
            sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (sz < 0 || sz > 65536) {
                fclose(fp);
                fprintf(stderr,
                    "Error: SQL file '%s' is too large (%ld bytes, max 65536).\n"
                    "  Split the file or use --stdin for large inputs.\n"
                    "  Run 'actagamma_db db help' for full usage.\n",
                    fpath, sz);
                return EXIT_INVALID;
            }
            sql_buf = malloc((size_t)sz + 1);
            if (!sql_buf) {
                fclose(fp);
                fprintf(stderr,
                    "Error: memory allocation failed.\n");
                return EXIT_INVALID;
            }
            size_t rd = fread(sql_buf, 1, (size_t)sz, fp);
            fclose(fp);
            sql_buf[rd] = '\0';
            sql_ptr = sql_buf;
        } else if (use_stdin) {
            VLOG(2, "  reading from stdin");
            sql_buf = malloc(65537);
            if (!sql_buf) {
                fprintf(stderr,
                    "Error: memory allocation failed.\n");
                return EXIT_INVALID;
            }
            size_t total = 0;
            size_t n;
            while ((n = fread(sql_buf + total, 1,
                              65536 - total, stdin)) > 0) {
                total += n;
                if (total >= 65536 - 1) {
                    free(sql_buf);
                    fprintf(stderr,
                        "Error: stdin input too large (max 65536 bytes).\n"
                        "  Pipe a smaller file or split the query.\n"
                        "  Run 'actagamma_db db help' for full usage.\n");
                    return EXIT_INVALID;
                }
            }
            sql_buf[total] = '\0';
            sql_ptr = sql_buf;
        }

        VLOG(3, "  sql_ptr=%p sql_len=%zu",
             (const void *)sql_ptr, strlen(sql_ptr));

        int rc = acta_db_exec(db, sql_ptr);

        VLOG(3, "  acta_db_exec → rc=%d last_error=%s",
             rc, acta_db_last_error(db) ? acta_db_last_error(db) : "(null)");

        free(sql_buf);

        if (rc != ACTA_DB_OK) {
            const char *msg = acta_db_last_error(db);
            VLOG(1, "  FAILED rc=%d (%s)", rc,
                 acta_db_strerror(rc));
            fprintf(stderr,
                "Error: SQL execution failed (rc=%d).\n"
                "  SQLite: %s\n"
                "  Check your SQL syntax. Run 'actagamma_db db help' for usage.\n",
                rc, msg ? msg : "(no detail)");
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  ok");
        exec_output(gopts);
        return EXIT_OK;
    }

    /* ── version ──────────────────────────────────────────────────── */
    if (strcmp(action, "version") == 0) {
        const char *ver = sqlite3_libversion();

        VLOG(1, "db version: %s", ver);

        if (gopts->table)
            fprintf(stdout, "SQLite %s\n", ver);
        else
            fprintf(stdout, "{\"version\":\"%s\"}\n", ver);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("db", action, "actagamma_db db help",
                          db_actions, DB_ACTIONS);
}
