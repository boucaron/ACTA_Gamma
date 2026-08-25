#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/* ── verbose logging to stderr (levels are cumulative) ────────────── */

static const global_opts_t *vlog_gopts;

#define VLOG(lvl, fmt, ...)                                              \
    do {                                                                 \
        if (vlog_gopts && vlog_gopts->verbose >= (lvl)) {                 \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);     \
        }                                                                \
    } while (0)

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
    { "exec",    "execute raw SQL"              },
    { "version", "print SQLite library version" },
};
#define DB_ACTIONS (sizeof(db_actions) / sizeof(db_actions[0]))

int cmd_db(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
           db_t *db)
{
    vlog_gopts = gopts;

    /* ── exec ─────────────────────────────────────────────────────── */
    if (strcmp(action, "exec") == 0) {
        const char *sql  = cmd_args_flag(ga, "sql",   1);
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
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"no SQL source: provide a positional argument, --sql, --file, or --stdin\"}\n");
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
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"cannot open file\"}\n");
                return EXIT_INVALID;
            }
            long sz = 0;
            fseek(fp, 0, SEEK_END);
            sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (sz < 0 || sz > 65536) {
                fclose(fp);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"SQL file too large (max 64 KiB)\"}\n");
                return EXIT_INVALID;
            }
            sql_buf = malloc((size_t)sz + 1);
            if (!sql_buf) {
                fclose(fp);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_ALLOC\",\"code\":-6,"
                    "\"message\":\"allocation failure\"}\n");
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
                    "{\"error\":\"ACTA_DB_ERR_ALLOC\",\"code\":-6,"
                    "\"message\":\"allocation failure\"}\n");
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
                        "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                        "\"message\":\"stdin input too large (max 64 KiB)\"}\n");
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
                "{\"error\":\"ACTA_DB_ERR_SQL\",\"code\":-5,"
                "\"message\":\"%s\"}\n",
                msg ? msg : "unknown sql error");
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  ok");
        exec_output(gopts);
        return EXIT_OK;
    }

    /* ── version ──────────────────────────────────────────────────── */
    if (strcmp(action, "version") == 0) {
        const char *ver = "test";

        VLOG(1, "db version: %s", ver);

        if (gopts->table)
            fprintf(stdout, "SQLite %s\n", ver);
        else
            fprintf(stdout, "{\"version\":\"%s\"}\n", ver);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "db: unknown action '%s'", action ? action : "(null)");
    return action_err("db", action, db_actions, DB_ACTIONS);
}
