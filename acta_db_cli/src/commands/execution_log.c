#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── verbose logging to stderr (levels are cumulative) ──────────────
 *
 *  Level 0  – silent (default)
 *  Level 1  – action summary
 *  Level 2  – parameter/field dump
 *  Level 3  – raw internal trace (pointers, raw rc)
 *
 *  All diagnostics → stderr so stdout stays pipe-safe.
 *
 *  Usage: set vlog_gopts at the top of cmd_execution_log, then call
 *         VLOG(1, "..."), VLOG(2, "...") etc. from anywhere in the
 *         translation unit, including helper functions.
 */

static const global_opts_t *vlog_gopts;   /* set once per cmd_* call */

#define VLOG(lvl, fmt, ...)                                              \
    do {                                                                 \
        if (vlog_gopts && vlog_gopts->verbose >= (lvl)) {                 \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);     \
        }                                                                \
    } while (0)

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_el_fields(const char *tag, const execution_log_t *c)
{
    VLOG(2, "%s: id=%d execution_id=%d level=%s event=%s message=%s metadata=%s created_at=%s",
         tag,
         c->id,
         c->execution_id,
         c->level        ? c->level        : "(null)",
         c->event        ? c->event        : "(null)",
         c->message      ? c->message      : "(null)",
         c->metadata     ? c->metadata     : "(null)",
         c->created_at   ? c->created_at   : "(null)");
}

static void vlog_el_raw(const char *tag, const execution_log_t *c, int rc)
{
    VLOG(3, "%s: el=%p id=%d rc=%d",
         tag, (const void *)c, c ? c->id : -1, rc);
}

/* ── execution_log_t → JSON object ────────────────────────────────── */

static void el_to_json(FILE *f, const execution_log_t *c, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", c->id);
    }
    if ((!fl || fields_has(fl, "execution_id")) && !(gopts->no_nulls && c->execution_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"execution_id\":%d", c->execution_id);
    }
    if ((!fl || fields_has(fl, "level")) && !(gopts->no_nulls && !c->level)) {
        if (shown++) fputs(", ", f);
        fputs("\"level\":", f);
        if (c->level) json_str(f, c->level); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "event")) && !(gopts->no_nulls && !c->event)) {
        if (shown++) fputs(", ", f);
        fputs("\"event\":", f);
        if (c->event) json_str(f, c->event); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "message")) && !(gopts->no_nulls && !c->message)) {
        if (shown++) fputs(", ", f);
        fputs("\"message\":", f);
        if (c->message) json_str(f, c->message); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "metadata")) && !(gopts->no_nulls && !c->metadata)) {
        if (shown++) fputs(", ", f);
        fputs("\"metadata\":", f);
        if (c->metadata) json_str(f, c->metadata); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !c->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (c->created_at) json_str(f, c->created_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void el_table(FILE *f, const execution_log_t *c, int header)
{
    if (header) {
        fprintf(f, " %4s  %10s  %-8s  %-20s  %-38s  %-20s  %-19s\n",
                "ID", "EXEC_ID", "LEVEL", "EVENT", "MESSAGE", "METADATA", "CREATED_AT");
        return;
    }
    char idb[16];
    char exb[16];
    snprintf(idb, sizeof idb, "%d", c->id);
    snprintf(exb, sizeof exb, "%d", c->execution_id);
    fprintf(f, " %4s  ", idb);
    fprintf(f, " %10s  ", exb);
    tcol(f, c->level,         8);
    tcol(f, c->event,        20);
    tcol(f, c->message,      38);
    tcol(f, c->metadata,     20);
    tcol(f, c->created_at,   19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t execution_log_actions[] = {
    { "create", "create a new log entry"     },
    { "get",    "fetch a log entry by id"    },
    { "list",   "list log entries"           },
    { "count",  "count log entries"          }
};
#define EL_ACTIONS (sizeof(execution_log_actions) / sizeof(execution_log_actions[0]))

static int valid_level(const char *lvl)
{
    return lvl &&
        (strcmp(lvl, "debug") == 0 ||
         strcmp(lvl, "info")  == 0 ||
         strcmp(lvl, "warn")  == 0 ||
         strcmp(lvl, "error") == 0);
}


int cmd_execution_log(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        execution_log_t el = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        if (gopts->json_input) {
            char *blob = read_stdin_all();
            if (!blob) {
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"failed to read JSON input\"}\n");
                return EXIT_INVALID;
            }
            VLOG(1, "log create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_execution_log(blob, &el) != 0) {
                VLOG(1, "  JSON parse error");
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"invalid JSON body\"}\n");
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            const char *f_exec_id  = cmd_args_flag(ga, "execution_id", 1);
            const char *f_level    = cmd_args_flag(ga, "level", 1);
            const char *f_event    = cmd_args_flag(ga, "event", 1);
            const char *f_message  = cmd_args_flag(ga, "message", 1);
            const char *f_metadata = cmd_args_flag(ga, "metadata", 1);

            el.execution_id = f_exec_id ? atoi(f_exec_id) : 0;
            el.level        = (char *)f_level;
            el.event        = (char *)f_event;
            el.message      = (char *)f_message;
            el.metadata     = (char *)f_metadata;
        }

        VLOG(1, "log create: execution_id=%d level=%s event=%s",
             el.execution_id,
             el.level  ? el.level  : "(missing)",
             el.event  ? el.event  : "(missing)");

        VLOG(2, "  params: execution_id=%d level=%s event=%s message=%s metadata=%s "
                "fields=%s no_nulls=%d id_only=%d table=%d",
             el.execution_id,
             el.level    ? el.level    : "(null)",
             el.event    ? el.event    : "(null)",
             el.message  ? el.message  : "(null)",
             el.metadata ? el.metadata : "(null)",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p json_owned=%d el=%p level=%p event=%p message=%p metadata=%p",
             (const void *)ga, json_owned, (const void *)&el,
             (const void *)el.level, (const void *)el.event,
             (const void *)el.message, (const void *)el.metadata);

        /* required-field validation */
        if (!el.level) {
            VLOG(1, "  ERROR: missing required field 'level'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: level\"}\n");
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (!el.event) {
            VLOG(1, "  ERROR: missing required field 'event'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: event\"}\n");
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
         if (!valid_level(el.level)) {
            VLOG(1, "  ERROR: 'level' must be one of: debug, info, warn, error "
                    "(got '%s')",
                    el.level ? el.level : "(null)");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"level must be one of: debug, info, warn, error\"}\n");
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (el.execution_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'execution_id'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: execution_id\"}\n");
            ret = EXIT_INVALID;
            goto cleanup_create;
        }


        vlog_el_fields("  pre-create", &el);
        VLOG(3, "  el=%p &out_id=%p",
             (const void *)&el, (const void *)&el.id);

        int out_id = 0;
        int rc = acta_db_execution_log_create(db, &el, &out_id);

        VLOG(3, "  acta_db_execution_log_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = map_rc_to_exit(rc);
            goto cleanup_create;
        }

        VLOG(1, "  created log entry id=%d", out_id);

        if (gopts->id_only)
            fprintf(stdout, "%d\n", out_id);
        else
            fprintf(stdout, "{\"id\":%d}\n", out_id);

        ret = EXIT_OK;
        goto cleanup_create;

    cleanup_create:
        if (json_owned) {
            free(el.level);
            free(el.event);
            free(el.message);
            free(el.metadata);
        }
        return ret;
    }


    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "log get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "log get: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "log get: fetching id=%d", id);

        int err = 0;
        execution_log_t *c = acta_db_execution_log_get(db, id, &err);

        VLOG(3, "  acta_db_execution_log_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_execution_log_free(c);
            return map_rc_to_exit(err);
        }
        if (!c) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

        vlog_el_fields("  result", c);
        vlog_el_raw("  raw", c, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", c->id);
        } else if (gopts->table) {
            el_table(stdout, NULL, 1);
            el_table(stdout, c, 0);
        } else {
            el_to_json(stdout, c, gopts);
            fputc('\n', stdout);
        }
        acta_db_execution_log_free(c);
        return EXIT_OK;
    }

    /* ── list <execution_id> ──────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *exec_id_str = cmd_args_next_positional(ga);
        if (!exec_id_str) {
            VLOG(1, "log list: ERROR missing <execution_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <execution_id>\"}\n");
            return EXIT_INVALID;
        }
        int execution_id = atoi(exec_id_str);
        if (execution_id <= 0) {
            VLOG(1, "log list: invalid execution_id=%s", exec_id_str);
            return EXIT_INVALID;
        }

        const char *f_level  = cmd_args_flag(ga, "level", 0);
        const char *s_off    = cmd_args_flag(ga, "offset", 1);
        const char *s_lim    = cmd_args_flag(ga, "limit", 1);
        const char *s_count  = cmd_args_flag(ga, "count", 0);

        int offset = 0, limit = 0;

        if (s_off) {
            char *end;
            long v = strtol(s_off, &end, 10);
            if (*end || v < 0) {
                VLOG(1, "  ERROR: --offset must be a non-negative integer, got '%s'", s_off);
                return EXIT_INVALID;
            }
            offset = (int)v;
        }
        if (s_lim) {
            char *end;
            long v = strtol(s_lim, &end, 10);
            if (*end || v < 0) {
                VLOG(1, "  ERROR: --limit must be a non-negative integer, got '%s'", s_lim);
                return EXIT_INVALID;
            }
            limit = (int)v;   /* 0 = no limit (documented) */
        }

        VLOG(1, "log list: execution_id=%d level=%s offset=%d limit=%d",
             execution_id,
             f_level ? f_level : "(any)",
             offset, limit);

        VLOG(2, "  full: execution_id=%d level=%s offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             execution_id,
             f_level ? f_level : "(null)",
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  exec_id=%d level=%p offset=%d limit=%d",
             execution_id, (const void *)f_level, offset, limit);

        if (gopts->count || s_count) {
            int err = 0;
            int n = acta_db_execution_log_count(db, execution_id, f_level, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        execution_log_t **items = acta_db_execution_log_list_by_execution(
            db, execution_id, f_level, offset, limit, &out_count, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_execution_log_list_free(items, out_count);
            return map_rc_to_exit(err);
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_el_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            el_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                el_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                el_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_execution_log_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count <execution_id> ─────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *exec_id_str = cmd_args_next_positional(ga);
        if (!exec_id_str) {
            VLOG(1, "log count: ERROR missing <execution_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <execution_id>\"}\n");
            return EXIT_INVALID;
        }
        int execution_id = atoi(exec_id_str);
        if (execution_id <= 0) {
            VLOG(1, "log count: invalid execution_id=%s", exec_id_str);
            return EXIT_INVALID;
        }

        const char *f_level = cmd_args_flag(ga, "level", 0);

        VLOG(1, "log count: execution_id=%d level=%s",
             execution_id,
             f_level ? f_level : "(any)");

        VLOG(2, "  level=%p execution_id=%d",
             (const void *)f_level, execution_id);

        int err = 0;
        int n = acta_db_execution_log_count(db, execution_id, f_level, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "execution_log: unknown action '%s'", action ? action : "(null)");
    return action_err("execution_log", action, execution_log_actions, EL_ACTIONS);
}
