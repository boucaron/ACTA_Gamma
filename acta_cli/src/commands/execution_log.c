#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════ */
/*  Usage / help                                                       */
/* ══════════════════════════════════════════════════════════════════ */

/* Non-static: the global dispatch layer can call this for
 *   acta_cli log --help                                          */
void execution_log_usage(FILE *f)
{
    fputs(
"Usage: acta_cli log <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new log entry\n"
"  get       Fetch a log entry by id\n"
"  list      List log entries for an execution\n"
"  count     Count log entries for an execution\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n"
"== create ===========================================================\n"
"  Create a new execution log entry.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli log create \\\n"
"      --execution_id 1 --level info \\\n"
"      --event \"stage_started\" \\\n"
"      --message \"Compiling module X\" \\\n"
"      --metadata '{\"module\":\"X\"}'\n"
"        <- flag-based\n"
"\n"
"    cat entry.json | acta_cli log create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --execution_id <int>   Owning execution (must be > 0)\n"
"    --level <str>          debug | info | warn | error\n"
"    --event <str>          Event name (e.g. \"stage_started\")\n"
"\n"
"  Optional fields:\n"
"    --message <str>        Human-readable detail\n"
"    --metadata <json>      Arbitrary JSON metadata\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the entry as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose [N]        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single log entry by its primary key.\n"
"\n"
"    acta_cli log get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== list <execution_id> ==============================================\n"
"  List log entries belonging to an execution.\n"
"\n"
"    acta_cli log list 1\n"
"    acta_cli log list 1 --level warn --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --level <str>        Filter by level (debug|info|warn|error)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count <execution_id> =============================================\n"
"  Count log entries for an execution.\n"
"\n"
"    acta_cli log count 1\n"
"    acta_cli log count 1 --level error\n"
"\n"
"  Options:\n"
"    --level <str>        Filter by level (debug|info|warn|error)\n"
"\n"
"Global options:\n"
"  --table            columnar / plain output instead of JSON\n"
"  --verbose [N]      debug level 0-3 (diagnostics on stderr)\n"
"  --fields <csv>     comma-separated field whitelist\n"
"  --no_nulls         omit null-valued fields from JSON output\n"
"  --id_only          print only the id (create / get)\n"
"\n", f);
}

/* ── per-action usage snippets (printed to stderr on arg errors) ──── */

static void usage_create(FILE *f)
{
    fputs(
"== create ===========================================================\n"
"  Create a new execution log entry.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli log create \\\n"
"      --execution_id 1 --level info \\\n"
"      --event \"stage_started\" \\\n"
"      --message \"Compiling module X\" \\\n"
"      --metadata '{\"module\":\"X\"}'\n"
"        <- flag-based\n"
"\n"
"    cat entry.json | acta_cli log create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --execution_id <int>   Owning execution (must be > 0)\n"
"    --level <str>          debug | info | warn | error\n"
"    --event <str>          Event name (e.g. \"stage_started\")\n"
"\n"
"  Optional fields:\n"
"    --message <str>        Human-readable detail\n"
"    --metadata <json>      Arbitrary JSON metadata\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the entry as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose [N]        debug level 0-3 (stderr)\n", f);
}

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single log entry by its primary key.\n"
"\n"
"    acta_cli log get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_list(FILE *f)
{
    fputs(
"== list <execution_id> ==============================================\n"
"  List log entries belonging to an execution.\n"
"\n"
"    acta_cli log list 1\n"
"    acta_cli log list 1 --level warn --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --level <str>        Filter by level (debug|info|warn|error)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --stream             NDJSON: one JSON object per line; pages\n"
"                         internally until exhausted (P5)\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_count(FILE *f)
{
    fputs(
"== count <execution_id> =============================================\n"
"  Count log entries for an execution.\n"
"\n"
"    acta_cli log count 1\n"
"    acta_cli log count 1 --level error\n"
"\n"
"  Options:\n"
"    --level <str>        Filter by level (debug|info|warn|error)\n", f);
}

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

/* free_row adapter for load_row_or_notfound (void* signature). */
static void el_free_wrap(void *c)
{
    acta_db_execution_log_free((execution_log_t *)c);
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
    { "count",  "count log entries"          },
    { "help",   "show this help"             },
};
#define EL_ACTIONS (sizeof(execution_log_actions) / sizeof(execution_log_actions[0]))

/* P0: print the help section for one log action.
 * 0 = printed, -1 = unknown action. */
int log_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "create")  == 0) usage_create(out);
    else if (strcmp(action, "get")     == 0) usage_get(out);
    else if (strcmp(action, "list")    == 0) usage_list(out);
    else if (strcmp(action, "count")   == 0) usage_count(out);
    else return -1;
    return 0;
}

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
    /* ── help: whole entity, or one action via `log help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (log_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("log", sub, "acta_cli log help",
                                  execution_log_actions, EL_ACTIONS);
        }
        execution_log_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        execution_log_t el = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_create(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "log create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_execution_log(blob, &el) != 0) {
                VLOG(1, "  JSON parse error");
                emit_error("invalid JSON body");
                usage_create(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            const char *f_level    = cmd_args_flag(ga, "level", 1);
            const char *f_event    = cmd_args_flag(ga, "event", 1);
            const char *f_message  = cmd_args_flag(ga, "message", 1);
            const char *f_metadata = cmd_args_flag(ga, "metadata", 1);

            if (parse_nonneg_int_flag(ga, "execution_id", &el.execution_id, 1,
                                      usage_create, "log create") < 0) {
                ret = EXIT_INVALID;
                goto cleanup_create;
            }
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
            emit_error("missing required field: level");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (!el.event) {
            VLOG(1, "  ERROR: missing required field 'event'");
            emit_error("missing required field: event");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (!valid_level(el.level)) {
            VLOG(1, "  ERROR: 'level' must be one of: debug, info, warn, error "
                    "(got '%s')",
                    el.level ? el.level : "(null)");
            emit_error("level must be one of: debug, info, warn, error");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (el.execution_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'execution_id'");
            emit_error("missing required field: execution_id");
            usage_create(stderr);
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
            ret = finish_op_error(db, rc, "execution_log create");
            goto cleanup_create;
        }

        VLOG(1, "  created log entry id=%d", out_id);

        emit_ok_id(gopts, out_id);

        ret = EXIT_OK;
        goto cleanup_create;

    cleanup_create:
        if (json_owned) {
            free(el.level);
            free(el.event);
            free(el.message);
            free(el.metadata);
            free(el.created_at);
        }
        return ret;
    }


    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_get, "log get", &id))
            return EXIT_INVALID;

        VLOG(1, "log get: fetching id=%d", id);

        int err = 0;
        execution_log_t *c = acta_db_execution_log_get(db, id, &err);

        VLOG(3, "  acta_db_execution_log_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        int rc = load_row_or_notfound(db, err, c, id, el_free_wrap,
                                      "execution_log get", "execution_log");
        if (rc)
            return rc;

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
        /* KI-6: --id_only is a single-row modifier (get); list actions
         * reject it with exit 4 instead of silently ignoring it. */
        if (gopts->id_only) {
            emit_error("log list: --id_only is not supported; remove the "
                       "flag (use the JSON rows, --count, --table, or "
                       "--stream)");
            return EXIT_INVALID;
        }

        int execution_id;
        if (!parse_id_positional(ga, "execution_id", usage_list,
                                 "log list", &execution_id))
            return EXIT_INVALID;

        const char *f_level  = cmd_args_flag(ga, "level", 1);

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_list, "log list") < 0)
            return EXIT_INVALID;

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

        if (gopts->count) {
            int err = 0;
            int n = acta_db_execution_log_count(db, execution_id, f_level, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "execution_log count");
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        /* P5: --stream — NDJSON (one JSON object per line), paging
         * internally until exhausted: no manual --offset loop needed
         * for bulk export. */
        if (gopts->stream) {
            if (gopts->count || gopts->table || gopts->id_only) {
                emit_error("conflicting output modes: --stream is "
                           "incompatible with --count, --table and --id_only");
                execution_log_usage(stderr);
                return EXIT_INVALID;
            }
            int emitted = 0;
            for (;;) {
                int want = (limit > 0) ? limit - emitted : 0;
                int n = 0, e2 = 0;
                execution_log_t **items =
                    acta_db_execution_log_list_by_execution(
                        db, execution_id, f_level, offset + emitted, want,
                        &n, &e2);
                if (e2 != ACTA_DB_OK) {
                    acta_db_execution_log_list_free(items, n);
                    return finish_op_error(db, e2, "execution_log list");
                }
                for (int i = 0; i < n; i++) {
                    el_to_json(stdout, items[i], gopts);
                    fputc('\n', stdout);
                }
                acta_db_execution_log_list_free(items, n);
                emitted += n;
                if ((limit > 0 && emitted >= limit) || n == 0 || n < want)
                    break;
            }
            VLOG(1, "  stream: %d item(s) emitted", emitted);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        execution_log_t **items = acta_db_execution_log_list_by_execution(
            db, execution_id, f_level, offset, limit, &out_count, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_execution_log_list_free(items, out_count);
            return finish_op_error(db, err, "execution_log list");
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
        int execution_id;
        if (!parse_id_positional(ga, "execution_id", usage_count,
                                 "log count", &execution_id))
            return EXIT_INVALID;

        const char *f_level = cmd_args_flag(ga, "level", 1);

        VLOG(1, "log count: execution_id=%d level=%s",
             execution_id,
             f_level ? f_level : "(any)");

        VLOG(2, "  level=%p execution_id=%d",
             (const void *)f_level, execution_id);

        int err = 0;
        int n = acta_db_execution_log_count(db, execution_id, f_level, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "execution_log count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    
    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("execution_log", action, "acta_cli execution_log help",
                          execution_log_actions, EL_ACTIONS);
   
}
