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

/*
 * P0: the per-action sections below are the single source of truth
 * for the db help text — db_usage() composes them, and
 * db_help_for_action() prints one of them. `db help <action>` and
 * `db <action> --help` therefore cannot drift.
 */

static void usage_exec(FILE *f)
{
    fputs(
"== exec =========================================================\n"
"  Execute a single mutating statement (no SELECT / query support).\n"
"  A SELECT statement is rejected with exit 4 (query statements are\n"
"  not supported; use the entity list actions to read rows).\n"
"  Supported: INSERT, UPDATE, DELETE, CREATE, DROP, ALTER,\n"
"             TRUNCATE, REPLACE, and other write / DDL statements.\n"
"\n"
"  Provide SQL via one of (mutually exclusive, first wins):\n"
"\n"
"    acta_cli db exec \"INSERT INTO users (name) VALUES ('Ada');\"\n"
"        <- positional argument\n"
"\n"
"    acta_cli db exec --sql \"DELETE FROM sessions WHERE expires < now;\"\n"
"        <- --sql flag\n"
"\n"
"    acta_cli db exec --file /path/to/migration.sql\n"
"        <- --file flag (max 64 KiB)\n"
"\n"
"    acta_cli db exec --sql_stdin\n"
"    cat migration.sql | acta_cli db exec --sql_stdin\n"
"        <- --sql_stdin flag (max 64 KiB)\n"
"\n"
"  Options:\n"
"    --sql <text>       SQL text to execute\n"
"    --file <path>      Read SQL from a file (max 64 KiB)\n"
"    --sql_stdin        Read SQL from stdin (max 64 KiB)\n"
"    --table            print 'ok' instead of JSON\n"
"    --verbose <n>      debug level 0-3 (stderr)\n"
"\n"
"  stdout on success: {\"status\":\"ok\"}\n"
"  stderr on failure: single-line JSON {\"error\":\"ACTA_DB_ERR_*\",\n"
"  \"code\":<rc>,\"message\":\"...\"} (exit code mapped from rc)\n"
"\n", f);
}

static void usage_version(FILE *f)
{
    fputs(
"== version ======================================================\n"
"  acta_cli db version\n"
"  Prints the SQLite library version.\n"
"  Options:\n"
"    --table          print 'SQLite <ver>' instead of JSON\n"
"\n"
"  stdout: {\"version\":\"<version>\"}\n"
"\n", f);
}

void db_usage(FILE *f)
{
    fputs(
"Usage: acta_cli db <action> [options]\n"
"\n"
"Actions:\n"
"  exec      Execute mutating SQL (INSERT, UPDATE, DELETE, DDL, etc.)\n"
"  version   Print SQLite library version\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n", f);
    usage_exec(f);
    usage_version(f);
    fputs(
"Global options:\n"
"  --table          columnar / plain output instead of JSON\n"
"  --verbose <n>    debug level 0-3 (diagnostics on stderr)\n"
"\n", f);
}

/* P0: print the help section for one db action. 0 = printed, -1 = unknown. */
int db_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "exec") == 0)      usage_exec(out);
    else if (strcmp(action, "version") == 0) usage_version(out);
    else return -1;
    return 0;
}



/* ── helpers ───────────────────────────────────────────────────────── */

/* KI-5: skip leading whitespace, stray ';', and SQL comments, then
 * report whether the first statement keyword is SELECT. */
static int sql_first_statement_is_select(const char *sql)
{
    const char *p = sql;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ';')
            p++;
        if (p[0] == '-' && p[1] == '-') {   /* line comment */
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {   /* block comment */
            p += 2;
            while (*p && !(*p == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        break;
    }
    /* SELECT is six letters: S E L E C T — the terminator test is on
     * p[6], the first byte after the keyword. */
    return (p[0] == 'S' || p[0] == 's') &&
           p[1] == 'E' && p[2] == 'L' && p[3] == 'E' && p[4] == 'C' &&
           p[5] == 'T' &&
           (p[6] == '\0' || p[6] == ' ' || p[6] == '\t' ||
            p[6] == '\r' || p[6] == '\n');
}

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
    /* ── help: whole entity, or one action via `db help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (db_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("db", sub, "acta_cli db help",
                                  db_actions, DB_ACTIONS);
        }
        db_usage(stdout);
        return EXIT_OK;
    }

    /* ── exec ─────────────────────────────────────────────────────── */
    if (strcmp(action, "exec") == 0) {
        /* Sources, mutually exclusive, first wins in the order the
         * help lists them: positional, --sql, --file, --sql_stdin. */
        const char *pos_sql = cmd_args_next_positional(ga);
        const char *sql     = cmd_args_flag(ga, "sql",   1);
        const char *fpath   = cmd_args_flag(ga, "file",  1);
        /* KI-1: boolean flags must be read with cmd_args_has_flag;
         * cmd_args_flag(...) is NULL by construction for them. */
        const int   use_stdin = cmd_args_has_flag(ga, "sql_stdin");

        VLOG(1, "db exec: pos=%s sql=%s file=%s stdin=%d",
             pos_sql? pos_sql: "(null)",
             sql    ? sql    : "(null)",
             fpath  ? fpath  : "(null)",
             use_stdin);

        VLOG(2, "  gopts: fields=%s no_nulls=%d table=%d id_only=%d verbose=%d",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table, gopts->verbose);

        /* The global --stdin flag is consumed by parse_globals into
         * gopts->from_stdin before the handler sees it, so the entity
         * flag is --sql_stdin. Reject --stdin explicitly instead of
         * letting it silently fall through to "no SQL source". */
        if (gopts->from_stdin) {
            VLOG(1, "  ERROR: global --stdin used; db exec expects --sql_stdin");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "global --stdin is a JSON-input flag; read SQL from stdin "
                "with --sql_stdin (acta_cli db exec --sql_stdin)");
        }

        if (!pos_sql && !sql && !fpath && !use_stdin) {
            VLOG(1, "  ERROR: no SQL source (need positional, --sql, --file, or --sql_stdin)");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "no SQL source: provide SQL as a positional argument, "
                "--sql, --file, or --sql_stdin");
        }


        /* resolve SQL text */
        char   *sql_buf = NULL;
        const char *sql_ptr = NULL;

        if (pos_sql) {
            sql_ptr = pos_sql;
        } else if (sql) {
            sql_ptr = sql;
        } else if (fpath) {
            VLOG(2, "  reading file: %s", fpath);
            FILE *fp = fopen(fpath, "r");
            if (!fp) {
                VLOG(1, "  ERROR: cannot open file '%s'", fpath);
                char what[1024];
                snprintf(what, sizeof what,
                         "cannot open file '%s': check the path and permissions",
                         fpath);
                return finish_db_error(ACTA_DB_ERR_INVALID, what);
            }
            long sz = 0;
            fseek(fp, 0, SEEK_END);
            sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (sz < 0 || sz > 65536) {
                fclose(fp);
                char what[1024];
                snprintf(what, sizeof what,
                         "SQL file '%s' too large (%ld bytes, max 65536): "
                         "split the file or use --sql_stdin for large inputs",
                         fpath, sz);
                return finish_db_error(ACTA_DB_ERR_INVALID, what);
            }
            sql_buf = malloc((size_t)sz + 1);
            if (!sql_buf) {
                fclose(fp);
                return finish_db_error(ACTA_DB_ERR_ALLOC,
                                       "memory allocation failed");
            }
            size_t rd = fread(sql_buf, 1, (size_t)sz, fp);
            fclose(fp);
            sql_buf[rd] = '\0';
            sql_ptr = sql_buf;
        } else if (use_stdin) {
            VLOG(2, "  reading from stdin");
            sql_buf = malloc(65537);
            if (!sql_buf) {
                return finish_db_error(ACTA_DB_ERR_ALLOC,
                                       "memory allocation failed");
            }
            size_t total = 0;
            size_t n;
            while ((n = fread(sql_buf + total, 1,
                              65536 - total, stdin)) > 0) {
                total += n;
                if (total >= 65536 - 1) {
                    free(sql_buf);
                    return finish_db_error(ACTA_DB_ERR_INVALID,
                        "stdin input too large (max 65536 bytes): "
                        "pipe a smaller file or split the query");
                }
            }
            sql_buf[total] = '\0';
            sql_ptr = sql_buf;
        }

        if (sql_ptr == NULL || *sql_ptr == '\0') {
            free(sql_buf);
            VLOG(1, "  ERROR: empty SQL resolved from source");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "empty SQL: the source (positional, --sql, --file, or "
                "--sql_stdin) resolved to 0 bytes");
        }

        VLOG(3, "  sql_ptr=%p sql_len=%zu",
             (const void *)sql_ptr, strlen(sql_ptr));

        /* KI-5: db exec is mutating-only (per the help text and
         * cli_spec.md); reject SELECT before touching the DB instead
         * of running a silent query. */
        if (sql_first_statement_is_select(sql_ptr)) {
            VLOG(1, "  ERROR: SELECT statement rejected");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "db exec does not support SELECT / query statements; "
                "it executes mutating SQL only (INSERT, UPDATE, DELETE, "
                "CREATE, DROP, ALTER, ...). Use the entity list actions "
                "to query rows.");
        }

        int rc = acta_db_exec(db, sql_ptr);

        VLOG(3, "  acta_db_exec → rc=%d last_error=%s",
             rc, acta_db_last_error(db) ? acta_db_last_error(db) : "(null)");

        free(sql_buf);

        if (rc != ACTA_DB_OK) {
            const char *msg = acta_db_last_error(db);
            VLOG(1, "  FAILED rc=%d (%s)", rc,
                 acta_db_strerror(rc));
            char what[1024];
            snprintf(what, sizeof what, "SQL execution failed: %s",
                     msg ? msg : "(no detail)");
            return finish_db_error(rc, what);
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

    /* ── Unknown action: JSON contract line first (stderr line 1,
     *    scripts parse it), then the shared human "Unknown action"
     *    suggestion block ── */
    {
        char what[80];
        snprintf(what, sizeof what, "unknown action '%s'",
                 action ? action : "(null)");
        finish_db_error(ACTA_DB_ERR_INVALID, what);
    }
    return unknown_action("db", action, "acta_cli db help",
                          db_actions, DB_ACTIONS);
}
