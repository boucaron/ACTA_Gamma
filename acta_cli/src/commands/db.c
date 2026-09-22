#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>   /* getcwd */
#else
#include <unistd.h>   /* getcwd */
#endif

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
"    --verbose [N]      debug level 0-3 (stderr)\n"
"\n"
"  stdout on success: {\"status\":\"ok\"}\n"
"  stderr on failure: single-line JSON {\"error\":\"ACTA_DB_ERR_*\",\n"
"  \"code\":<rc>,\"message\":\"...\"} (exit code mapped from rc)\n"
"\n", f);
}

/* ── path identity for the self-backup guard ──────────────────────
 * Canonicalize `p` into `out` (caller-provided, outsz bytes): resolve
 * relative paths against the cwd, drop trailing slashes.  Used to
 * compare the backup target with the DB path as *files*, not as raw
 * strings — otherwise equivalent spellings (`./x` vs `x`, absolute
 * vs relative, case on Windows) would slip past the guard and back
 * the live database up onto itself. */
static void canon_path(const char *p, char *out, size_t outsz)
{
    size_t len = strlen(p);
    int is_abs = (p[0] == '/') ||
                 (len >= 3 && isalpha((unsigned char)p[0]) && p[1] == ':');
    if (!is_abs) {
        char cwd[256];
        if (getcwd(cwd, sizeof cwd) == NULL) { out[0] = '\0'; return; }
        snprintf(out, outsz, "%s/%s", cwd, p);
    } else {
        snprintf(out, outsz, "%s", p);
    }
    size_t n = strlen(out);
    while (n > 1 && out[n - 1] == '/') out[--n] = '\0';
}

static int same_file_path(const char *a, const char *b)
{
    char ca[512], cb[512];
    canon_path(a, ca, sizeof ca);
    canon_path(b, cb, sizeof cb);
    if (ca[0] == '\0' || cb[0] == '\0')
        return 0;
#ifdef _WIN32
    return _stricmp(ca, cb) == 0;   /* case-insensitive on Windows */
#else
    return strcmp(ca, cb) == 0;
#endif
}

static void usage_backup(FILE *f)
{
    fputs(
"== backup ========================================================\n"
"  Atomic, consistent snapshot of the open database into <target>.\n"
"  Written via the SQLite backup C API while the database stays open\n"
"  (WAL state is folded into the snapshot) — no need to close the\n"
"  GUI / CLI / runner first.\n"
"\n"
"    acta_cli db backup --to /path/to/acta_backup_YYYYMMDD.db\n"
"\n"
"  Rules:\n"
"    --to <path> is required.  The target must NOT exist (no silent\n"
"    overwrite); rotation is by dated name (acta_backup_YYYYMMDD.db).\n"
"    The target is validated strictly: non-empty, no quote / semicolon\n"
"    characters (plus no backslash on POSIX — on Windows the\n"
"    backslash is the normal path separator), not the same file as\n"
"    the DB path itself, and a path the process can create.\n"
"    After the copy, the backup is reopened on its own connection and\n"
"    PRAGMA quick_check is run; a backup that does not check is\n"
"    reported as failed (and not kept).\n"
"\n"
"  Options:\n"
"    --to <path>      backup target file (required)\n"
"    --table          print '<target> <N> bytes' instead of JSON\n"
"\n"
"  stdout on success: {\"target\":\"<path>\",\"bytes\":<size>,\n"
"  \"quick_check\":\"ok\"}\n"
"  stderr on failure: single-line JSON (exit code mapped from rc)\n"
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
"  backup    Atomic snapshot of the DB into --to <target>\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n", f);
    usage_exec(f);
    usage_version(f);
    usage_backup(f);
    fputs(
"Global options:\n"
"  --table          columnar / plain output instead of JSON\n"
"  --verbose [N]    debug level 0-3 (diagnostics on stderr)\n"
"\n", f);
}

/* P0: print the help section for one db action. 0 = printed, -1 = unknown. */
int db_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "exec") == 0)      usage_exec(out);
    else if (strcmp(action, "version") == 0) usage_version(out);
    else if (strcmp(action, "backup") == 0) usage_backup(out);
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
    { "backup",  "atomic snapshot of the DB into --to <target>" },
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
            if (!msg) msg = acta_db_errmsg(db);   /* KI-7: surface sqlite3_errmsg */
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

    /* ── backup ───────────────────────────────────────────────────── */
    if (strcmp(action, "backup") == 0) {
        const char *target = cmd_args_flag(ga, "to", 1);

        VLOG(1, "db backup: to=%s", target ? target : "(null)");

        /* The global --stdin is a JSON-input flag; db backup takes no
         * input.  Reject it explicitly instead of letting it fall
         * through (same rule as db exec). */
        if (gopts->from_stdin) {
            VLOG(1, "  ERROR: global --stdin used; db backup takes no input");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "global --stdin is a JSON-input flag; db backup takes no "
                "input");
        }

        if (!target || *target == '\0') {
            VLOG(1, "  ERROR: missing --to");
            return emit_cli_error(
                "db backup requires --to <target>");
        }

        /* Strict validation of the user-supplied target: reject the
         * characters that would be meaningful in SQL text (defense
         * in depth — the path reaches SQLite only via the backup C
         * API, never as SQL text).  Backslash is the native path
         * separator on Windows, so it is allowed there; on POSIX it
         * stays in the rejected set. */
        for (const char *p = target; *p; p++) {
            int bad = (*p == '\'' || *p == '"' || *p == ';');
#ifndef _WIN32
            bad = bad || (*p == '\\');
#endif
            if (bad) {
                VLOG(1, "  ERROR: invalid character in target");
                return emit_cli_error(
#ifdef _WIN32
                    "db backup target is invalid: quote or semicolon "
                    "characters are not allowed"
#else
                    "db backup target is invalid: quote, semicolon or "
                    "backslash characters are not allowed"
#endif
                );
            }
        }

        /* Reject the DB path itself (backing the database up onto
         * itself is a no-op trap).  Compared as canonicalized
         * absolute paths, not by raw spelling: `./x`, `x`, an absolute
         * spelling, and (on Windows) case differences all name the
         * same file. */
        const char *dbpath = acta_db_main_path(db);
        if (dbpath && same_file_path(target, dbpath)) {
            VLOG(1, "  ERROR: target is the DB path itself");
            return emit_cli_error(
                "db backup target must not be the database path itself");
        }

        /* No silent overwrite: the target must not exist.  The stat
         * probe doubles as the "path the process cannot create" check:
         * a stat failure other than ENOENT (bad directory, no
         * permission) means the target cannot be created either. */
        struct stat st;
        if (stat(target, &st) == 0) {
            VLOG(1, "  ERROR: target exists");
            return emit_cli_error(
                "db backup target already exists: no silent overwrite "
                "(choose a new name, e.g. acta_backup_YYYYMMDD.db)");
        }
        if (errno != ENOENT) {
            VLOG(1, "  ERROR: cannot probe target");
            return emit_cli_error(
                "cannot access db backup target (check the path and "
                "permissions)");
        }

        int err = 0;
        long long bytes = 0;
        int rc = acta_db_backup(db, target, &bytes, &err);

        if (rc != ACTA_DB_OK) {
            const char *msg = acta_db_last_error(db);
            VLOG(1, "  FAILED rc=%d (%s)", rc,
                 msg ? msg : "(no detail)");
            return finish_db_error(rc, msg ? msg : "backup failed");
        }

        VLOG(1, "  ok: %s (%lld bytes, quick_check ok)", target, bytes);
        if (gopts->table)
            fprintf(stdout, "%s %lld bytes\n", target, bytes);
        else {
            fputs("{\"target\":", stdout);
            json_str(stdout, target);
            fprintf(stdout, ",\"bytes\":%lld,\"quick_check\":\"ok\"}\n",
                     bytes);
        }
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
