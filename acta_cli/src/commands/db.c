#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include "schema_sql.h"
#include "migrations_sql.h"
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

static void usage_init(FILE *f)
{
    fputs(
"== init =========================================================\n"
"  Apply the canonical schema (the static, embedded copy of\n"
"  acta_db/schema.sql) to a fresh database file.  Takes no SQL\n"
"  input of any kind.\n"
"\n"
"    acta_cli db init\n"
"\n"
"  Behaviour:\n"
"    - Fresh file (no user tables): the canonical schema is applied\n"
"      and {\"status\":\"ok\"} is emitted.\n"
"    - Already schema'd file: idempotent no-op, {\"status\":\"ok\"}.\n"
"    - Partially applied or foreign file: fail closed with a clear\n"
"      error (exit 4) — the schema is never re-run on top of\n"
"      existing tables.\n"
"\n"
"  Options:\n"
"    --table            print 'ok' instead of JSON\n"
"    --verbose [N]      debug level 0-3 (stderr)\n"
"\n"
"  stdout on success: {\"status\":\"ok\"}\n"
"  stderr on failure: single-line JSON {\"error\":\"ACTA_DB_ERR_*\",\n"
"  \"code\":<rc>,\"message\":\"...\"} (exit code mapped from rc)\n"
"\n", f);
}
static void usage_migrate(FILE *f)
{
    fputs(
"== migrate ========================================================\n"
"  Apply the pending repo-static schema migrations (one static DDL\n"
"  file per version, acta_db/migrations/<version>.sql, embedded)\n"
"  to an existing database.  PRAGMA user_version is the recorded\n"
"  schema version (0.1 -> 1); each migration newer than the file's\n"
"  version is applied in ascending order, in its own BEGIN..COMMIT\n"
"  transaction, and sets user_version on success.  A failing\n"
"  migration rolls back itself; the file keeps its prior version.\n"
"  Takes no SQL input of any kind.\n"
"\n"
"    acta_cli db migrate\n"
"\n"
"  Behaviour:\n"
"    - Fresh file (user_version 0, no user tables): migrations apply\n"
"      from the first (0.1).\n"
"    - Already up to date: idempotent no-op, current version printed.\n"
"    - User tables present without a recorded version (foreign or\n"
"      pre-migration file): fail closed with a clear error (exit 4)\n"
"      — no migration is applied.\n"
"\n"
"  Options:\n"
"    --table            print 'ok' instead of JSON\n"
"    --verbose [N]      debug level 0-3 (stderr)\n"
"\n"
"  stdout on success: {\"status\":\"ok\",\"schema_version\":\"<v>\"}\n"
"  stderr on failure: single-line JSON (exit 4, failing migration\n"
"  named in the message)\n"
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
"  init      Apply the canonical schema to a fresh database file\n"
"  migrate   Apply pending repo-static schema migrations\n"
"  version   Print SQLite library version\n"
"  backup    Atomic snapshot of the DB into --to <target>\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n", f);
    usage_init(f);
    usage_migrate(f);
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
    if (strcmp(action, "init") == 0)      usage_init(out);
    else if (strcmp(action, "migrate") == 0) usage_migrate(out);
    else if (strcmp(action, "version") == 0) usage_version(out);
    else if (strcmp(action, "backup") == 0) usage_backup(out);
    else return -1;
    return 0;
}


/* ══════════════════════════════════════════════════════════════════ */
/*  Static migration apply                                             */
/* ══════════════════════════════════════════════════════════════════ */

/* Apply the pending migrations from `migs` (ascending `version`,
 * `n` entries) whose version is newer than the file's recorded
 * PRAGMA user_version.  Each migration runs in its own BEGIN..COMMIT
 * transaction; the user_version is set AFTER the commit (PRAGMA
 * user_version is a no-op inside a transaction).  A failing migration
 * rolls back itself, the file keeps its prior user_version, and
 * ACTA_DB_ERR_INVALID is returned with "migration <name> failed:
 * <detail>" written into `fail_msg` (when non-NULL).
 * *out_version receives the PRAGMA user_version integer (0.1 -> 1)
 * the file is at when the call returns: on success (ACTA_DB_OK) the
 * final applied version; on failure the last successfully applied
 * version (i.e. the prior version, unchanged).  The fresh-vs-foreign user_version-0 policy (no user
 * tables vs. user tables present) is the caller's decision — the
 * caller owns the table listing. */
int db_migrate_apply(db_t *db, const acta_migration_t *migs, int n,
                     int *out_version, char *fail_msg, size_t failmsgsz)
{
    if (!db || !out_version || (n > 0 && !migs))
        return ACTA_DB_ERR_INVALID;

    int uv = 0;
    if (acta_db_schema_version(db, &uv) != ACTA_DB_OK)
        return ACTA_DB_ERR_SQL;

    for (int i = 0; i < n; i++) {
        if (migs[i].sql == NULL || migs[i].version <= uv)
            continue;

        int rc = acta_db_begin(db);
        if (rc == ACTA_DB_OK)
            rc = acta_db_exec(db, migs[i].sql);
        if (rc == ACTA_DB_OK)
            rc = acta_db_commit(db);
        if (rc == ACTA_DB_OK)
            rc = acta_db_set_schema_version(db, migs[i].version);
        if (rc != ACTA_DB_OK) {
            acta_db_rollback(db);   /* no-op when the commit already ran */
            if (fail_msg && failmsgsz > 0) {
                char ver[8];
                snprintf(ver, sizeof ver, "0.%d", migs[i].version);
                const char *detail = acta_db_last_error(db);
                snprintf(fail_msg, failmsgsz, "migration %s failed: %s",
                         ver, detail ? detail : "(no detail)");
            }
            /* Report the version the file is currently at (the last
             * successfully applied migration; unchanged if none). */
            *out_version = uv;
            return ACTA_DB_ERR_INVALID;
        }
        uv = migs[i].version;
    }
    *out_version = uv;
    return ACTA_DB_OK;
}


/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t db_actions[] = {
    { "init",    "apply the canonical schema to a fresh database file" },
    { "migrate", "apply pending repo-static schema migrations"        },
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

    /* ── init ─────────────────────────────────────────────────────── */
    if (strcmp(action, "init") == 0) {
        /* db init takes no SQL input: reject any SQL-carrying source
         * explicitly instead of letting it fall through. */
        const char *pos_sql   = cmd_args_next_positional(ga);
        const char *sql       = cmd_args_flag(ga, "sql",   1);
        const char *fpath     = cmd_args_flag(ga, "file",  1);
        const int   use_stdin = cmd_args_has_flag(ga, "sql_stdin");

        VLOG(1, "db init: pos=%s sql=%s file=%s stdin=%d",
             pos_sql ? pos_sql : "(null)",
             sql     ? sql     : "(null)",
             fpath   ? fpath   : "(null)",
             use_stdin);

        if (gopts->from_stdin) {
            VLOG(1, "  ERROR: global --stdin used; db init takes no input");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "global --stdin is a JSON-input flag; db init takes no "
                "input (it applies the canonical embedded schema)");
        }

        if (pos_sql || sql || fpath || use_stdin) {
            VLOG(1, "  ERROR: SQL input rejected");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "db init takes no arguments: it applies the canonical "
                "embedded schema — there is no SQL to supply");
        }

        /* The canonical table set (acta_db/schema.sql). */
        static const char *const CANONICAL[] = {
            "model_folders", "models", "model_revisions",
            "skill_folders", "skills", "skill_revisions",
            "contexts", "executions", "execution_logs",
        };
        const int n_canon =
            (int)(sizeof CANONICAL / sizeof CANONICAL[0]);

        int err = 0, n = 0;
        char **tables = acta_db_user_tables(db, &n, &err);
        if (err != ACTA_DB_OK || tables == NULL) {
            char what[256];
            snprintf(what, sizeof what, "cannot list user tables: %s",
                     acta_db_last_error(db) ? acta_db_last_error(db)
                                            : "(no detail)");
            acta_db_user_tables_free(tables, n);
            return finish_db_error(err, what);
        }

        int have_canon = 0;
        for (int i = 0; i < n; i++) {
            for (int c = 0; c < n_canon; c++)
                if (strcmp(tables[i], CANONICAL[c]) == 0) {
                    have_canon++;
                    break;
                }
        }

        /* Record the schema version (PRAGMA user_version; 0.1 -> 1)
         * whenever the canonical schema ends up in place: a fresh
         * file gets it on application, and a legacy file that was
         * schema'd before versioning was introduced (uv 0, all
         * canonical tables present) is adopted here.  `db migrate`
         * is then a clean no-op on both. */
        int uv = 0;
        acta_db_schema_version(db, &uv);

        int rc;
        if (n == 0) {
            VLOG(1, "  fresh file: applying canonical schema");
            rc = acta_db_exec(db, ACTA_SCHEMA_SQL);
            if (rc == ACTA_DB_OK)
                rc = acta_db_set_schema_version(db, 1);
        } else if (have_canon == n_canon) {
            /* Already schema'd: short-circuit no-op (the shipped schema
             * uses bare CREATE TABLE, so it must not be re-run). */
            VLOG(1, "  already schema'd: no-op (%d user tables)", n);
            rc = ACTA_DB_OK;
            if (uv == 0)
                rc = acta_db_set_schema_version(db, 1);
        } else {
            VLOG(1, "  partial/foreign file: %d of %d canonical tables",
                 have_canon, n_canon);
            char what[256];
            snprintf(what, sizeof what,
                     "database is not fresh and not fully schema'd "
                     "(%d of %d canonical tables present): fail closed, "
                     "no schema applied",
                     have_canon, n_canon);
            acta_db_user_tables_free(tables, n);
            return finish_db_error(ACTA_DB_ERR_INVALID, what);
        }
        acta_db_user_tables_free(tables, n);

        if (rc != ACTA_DB_OK) {
            const char *msg = acta_db_last_error(db);
            VLOG(1, "  FAILED rc=%d (%s)", rc, msg ? msg : "(no detail)");
            return finish_db_error(rc, msg ? msg : "schema application failed");
        }

        VLOG(1, "  ok");
        if (gopts->table)
            fprintf(stdout, "ok\n");
        else
            fprintf(stdout, "{\"status\":\"ok\"}\n");
        return EXIT_OK;
    }

    /* ── migrate ─────────────────────────────────────────────────── */
    if (strcmp(action, "migrate") == 0) {
        /* db migrate takes no SQL input: reject any SQL-carrying
         * source explicitly instead of letting it fall through
         * (same rule as db init). */
        const char *pos_sql   = cmd_args_next_positional(ga);
        const char *sql       = cmd_args_flag(ga, "sql",   1);
        const char *fpath     = cmd_args_flag(ga, "file",  1);
        const int   use_stdin = cmd_args_has_flag(ga, "sql_stdin");

        if (gopts->from_stdin || pos_sql || sql || fpath || use_stdin) {
            VLOG(1, "  ERROR: SQL input rejected");
            return finish_db_error(ACTA_DB_ERR_INVALID,
                "db migrate takes no arguments: it applies the "
                "repo-static schema migrations — there is no SQL to "
                "supply");
        }

        int uv = 0;
        if (acta_db_schema_version(db, &uv) != ACTA_DB_OK)
            return finish_op_error(db, ACTA_DB_ERR_SQL,
                                   "schema_version read");

        /* user_version 0 = "no schema recorded": a truly fresh file has
         * no user tables (migrations apply from 0.1); any user tables
         * without a recorded version (foreign or pre-migration file)
         * fail closed instead of double-applying. */
        if (uv == 0) {
            int err = 0, nt = 0;
            char **tabs = acta_db_user_tables(db, &nt, &err);
            if (err != ACTA_DB_OK || tabs == NULL) {
                acta_db_user_tables_free(tabs, nt);
                return finish_db_error(err != ACTA_DB_OK ? err
                                    : ACTA_DB_ERR_SQL,
                                    "cannot list user tables");
            }
            if (nt > 0) {
                acta_db_user_tables_free(tabs, nt);
                VLOG(1, "  ERROR: user_version 0 with %d user tables",
                     nt);
                return finish_db_error(ACTA_DB_ERR_INVALID,
                    "file has user tables but no recorded schema version "
                    "(PRAGMA user_version = 0): not an ACTA Gamma database "
                    "(or a pre-migration file) — fail closed, no "
                    "migration applied");
            }
            acta_db_user_tables_free(tabs, nt);
        }

        const int n_migs =
            (int)(sizeof ACTA_MIGRATIONS / sizeof ACTA_MIGRATIONS[0] - 1);
        int out_v = 0;
        char fail[256];
        int rc = db_migrate_apply(db, ACTA_MIGRATIONS, n_migs,
                                 &out_v, fail, sizeof fail);
        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d (%s)", rc, fail[0] ? fail : "(no detail)");
            return finish_db_error(rc, fail[0] ? fail : "migration failed");
        }

        VLOG(1, "  ok: schema version 0.%d", out_v);
        if (gopts->table)
            fprintf(stdout, "ok\n");
        else
            fprintf(stdout,
                     "{\"status\":\"ok\",\"schema_version\":\"0.%d\"}\n", out_v);
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
         * through (same rule as db init). */
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
