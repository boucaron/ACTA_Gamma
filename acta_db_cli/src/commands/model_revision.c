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
 *   actagamma_db model_revision --help
 */
void model_revision_usage(FILE *f)
{
    fputs(
"Usage: actagamma_db model_revision <action> [options]\n"
"\n"
"Actions:\n"
"  get <id>         Fetch a model revision by id\n"
"  get-latest <model_id>  Fetch the latest revision for a model\n"
"  list <model_id>  List revisions for a model\n"
"  count <model_id> Count revisions for a model\n"
"  help             Show this help\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single model revision by its primary key.\n"
"\n"
"    actagamma_db model_revision get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== get-latest <model_id> ============================================\n"
"  Fetch the latest revision for a given model.\n"
"\n"
"    actagamma_db model_revision get-latest 7\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== list <model_id> =================================================\n"
"  List model revisions for a model.\n"
"\n"
"    actagamma_db model_revision list 7\n"
"    actagamma_db model_revision list 7 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count <model_id> ================================================\n"
"  Count model revisions for a model.\n"
"\n"
"    actagamma_db model_revision count 7\n"
"\n"
"Global options:\n"
"  --table              columnar / plain output instead of JSON\n"
"  --verbose <n>        debug level 0-3 (diagnostics on stderr)\n"
"  --fields <csv>       comma-separated field whitelist\n"
"  --no_nulls           omit null-valued fields from JSON output\n"
"  --id_only            print only the id (get / get-latest)\n"
"\n", f);
}

/* ── per-action usage snippets (printed to stderr on arg errors) ──── */

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single model revision by its primary key.\n"
"\n"
"    actagamma_db model_revision get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_get_latest(FILE *f)
{
    fputs(
"== get-latest <model_id> ============================================\n"
"  Fetch the latest revision for a given model.\n"
"\n"
"    actagamma_db model_revision get-latest 7\n"
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
"== list <model_id> =================================================\n"
"  List model revisions for a model.\n"
"\n"
"    actagamma_db model_revision list 7\n"
"    actagamma_db model_revision list 7 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_count(FILE *f)
{
    fputs(
"== count <model_id> ================================================\n"
"  Count model revisions for a model.\n"
"\n"
"    actagamma_db model_revision count 7\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_rev_fields(const char *tag, const model_revision_t *r)
{
    VLOG(2, "%s: id=%d model_id=%d revision=%d folder_id=%d name=%s "
         "description=%s backend=%s base_url=%s model_identifier=%s "
         "configuration=%s created_at=%s updated_at=%s deleted_at=%s",
         tag,
         r->id,
         r->model_id,
         r->revision,
         r->folder_id,
         r->name              ? r->name              : "(null)",
         r->description       ? r->description       : "(null)",
         r->backend           ? r->backend           : "(null)",
         r->base_url          ? r->base_url          : "(null)",
         r->model_identifier  ? r->model_identifier  : "(null)",
         r->configuration     ? r->configuration     : "(null)",
         r->created_at        ? r->created_at        : "(null)",
         r->updated_at        ? r->updated_at        : "(null)",
         r->deleted_at        ? r->deleted_at        : "(null)");
}

static void vlog_rev_raw(const char *tag, const model_revision_t *r, int rc)
{
    VLOG(3, "%s: rev=%p id=%d model_id=%d revision=%d rc=%d",
         tag, (const void *)r,
         r ? r->id : -1,
         r ? r->model_id : -1,
         r ? r->revision : -1,
         rc);
}

/* ── model_revision_t → JSON object ───────────────────────────────── */

static void rev_to_json(FILE *f, const model_revision_t *r, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", r->id);
    }
    if ((!fl || fields_has(fl, "model_id")) && !(gopts->no_nulls && r->model_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"model_id\":%d", r->model_id);
    }
    if ((!fl || fields_has(fl, "revision")) && !(gopts->no_nulls && r->revision == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"revision\":%d", r->revision);
    }
    if ((!fl || fields_has(fl, "folder_id")) && !(gopts->no_nulls && r->folder_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"folder_id\":%d", r->folder_id);
    }
    if ((!fl || fields_has(fl, "name")) && !(gopts->no_nulls && !r->name)) {
        if (shown++) fputs(", ", f);
        fputs("\"name\":", f);
        if (r->name) json_str(f, r->name); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "description")) && !(gopts->no_nulls && !r->description)) {
        if (shown++) fputs(", ", f);
        fputs("\"description\":", f);
        if (r->description) json_str(f, r->description); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "backend")) && !(gopts->no_nulls && !r->backend)) {
        if (shown++) fputs(", ", f);
        fputs("\"backend\":", f);
        if (r->backend) json_str(f, r->backend); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "base_url")) && !(gopts->no_nulls && !r->base_url)) {
        if (shown++) fputs(", ", f);
        fputs("\"base_url\":", f);
        if (r->base_url) json_str(f, r->base_url); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "model_identifier")) && !(gopts->no_nulls && !r->model_identifier)) {
        if (shown++) fputs(", ", f);
        fputs("\"model_identifier\":", f);
        if (r->model_identifier) json_str(f, r->model_identifier); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "configuration")) && !(gopts->no_nulls && !r->configuration)) {
        if (shown++) fputs(", ", f);
        fputs("\"configuration\":", f);
        if (r->configuration) json_str(f, r->configuration); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !r->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (r->created_at) json_str(f, r->created_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "updated_at")) && !(gopts->no_nulls && !r->updated_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"updated_at\":", f);
        if (r->updated_at) json_str(f, r->updated_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "deleted_at")) && !(gopts->no_nulls && !r->deleted_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"deleted_at\":", f);
        if (r->deleted_at) json_str(f, r->deleted_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void rev_table(FILE *f, const model_revision_t *r, int header)
{
    if (header) {
        fprintf(f, " %4s  %8s  %8s  %7s  %-20s  %-10s  %-30s  %-19s\n",
                "ID", "MODEL", "REV", "FOLDER", "NAME", "BACKEND", "MODEL_IDENTIFIER", "CREATED_AT");
        return;
    }
    char idb[16];
    char midb[16];
    char revb[16];
    char foldb[16];
    snprintf(idb,   sizeof idb,   "%d", r->id);
    snprintf(midb,  sizeof midb,  "%d", r->model_id);
    snprintf(revb,  sizeof revb,  "%d", r->revision);
    snprintf(foldb, sizeof foldb, "%d", r->folder_id);
    fprintf(f, " %4s  ", idb);
    fprintf(f, " %8s  ", midb);
    fprintf(f, " %8s  ", revb);
    fprintf(f, " %7s  ", foldb);
    tcol(f, r->name,              20);
    tcol(f, r->backend,           10);
    tcol(f, r->model_identifier,  30);
    tcol(f, r->created_at,        19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t model_revision_actions[] = {
    { "get",        "fetch a model revision by id"          },
    { "get-latest", "fetch the latest revision for a model" },
    { "list",       "list revisions for a model"            },
    { "count",      "count revisions for a model"           },
    { "help",       "show this help"                        },
};
#define REV_ACTIONS (sizeof(model_revision_actions) / sizeof(model_revision_actions[0]))

int cmd_model_revision(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                       db_t *db)
{
    /* ── help (subcommand-level; only the bare word "help") ──────── */
    if (strcmp(action, "help") == 0) {
        model_revision_usage(stdout);
        return EXIT_OK;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model_revision get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_get(stderr);
            return EXIT_INVALID;
        }
        int id;
        if (!parse_positive_id(id_str, &id)) {
            VLOG(1, "model_revision get: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_get(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_revision get: fetching id=%d", id);

        int err = 0;
        model_revision_t *r = acta_db_model_revision_get(db, id, &err);

        VLOG(3, "  acta_db_model_revision_get(%d) → ptr=%p err=%d",
             id, (const void *)r, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_model_revision_free(r);
            return finish_op_error(db, err, "model_revision get");
        }
        if (!r) {
            VLOG(1, "  not found (id=%d)", id);
            return finish_db_error(ACTA_DB_ERR_NOT_FOUND, "model_revision not found");
        }

        vlog_rev_fields("  result", r);
        vlog_rev_raw("  raw", r, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", r->id);
        } else if (gopts->table) {
            rev_table(stdout, NULL, 1);
            rev_table(stdout, r, 0);
        } else {
            rev_to_json(stdout, r, gopts);
            fputc('\n', stdout);
        }
        acta_db_model_revision_free(r);
        return EXIT_OK;
    }

    /* ── get-latest <model_id> ────────────────────────────────────── */
    if (strcmp(action, "get-latest") == 0) {
        const char *model_id_str = cmd_args_next_positional(ga);
        if (!model_id_str) {
            VLOG(1, "model_revision get-latest: ERROR missing <model_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <model_id>\"}\n");
            usage_get_latest(stderr);
            return EXIT_INVALID;
        }
        int model_id;
        if (!parse_positive_id(model_id_str, &model_id)) {
            VLOG(1, "model_revision get-latest: invalid model_id=%s", model_id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <model_id>: must be a positive integer\"}\n");
            usage_get_latest(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_revision get-latest: fetching latest for model_id=%d", model_id);

        int err = 0;
        model_revision_t *r = acta_db_model_revision_get_latest(db, model_id, &err);

        VLOG(3, "  acta_db_model_revision_get_latest(%d) → ptr=%p err=%d",
             model_id, (const void *)r, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_model_revision_free(r);
            return finish_op_error(db, err, "model_revision latest");
        }
        if (!r) {
            VLOG(1, "  no revisions found (model_id=%d)", model_id);
            return finish_db_error(ACTA_DB_ERR_NOT_FOUND, "no revisions found");
        }

        vlog_rev_fields("  result", r);
        vlog_rev_raw("  raw", r, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", r->id);
        } else if (gopts->table) {
            rev_table(stdout, NULL, 1);
            rev_table(stdout, r, 0);
        } else {
            rev_to_json(stdout, r, gopts);
            fputc('\n', stdout);
        }
        acta_db_model_revision_free(r);
        return EXIT_OK;
    }

    /* ── list <model_id> ──────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *model_id_str = cmd_args_next_positional(ga);
        if (!model_id_str) {
            VLOG(1, "model_revision list: ERROR missing <model_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <model_id>\"}\n");
            usage_list(stderr);
            return EXIT_INVALID;
        }
        int model_id;
        if (!parse_positive_id(model_id_str, &model_id)) {
            VLOG(1, "model_revision list: invalid model_id=%s", model_id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <model_id>: must be a positive integer\"}\n");
            usage_list(stderr);
            return EXIT_INVALID;
        }

        const char *s_off   = cmd_args_flag(ga, "offset", 1);
        const char *s_lim   = cmd_args_flag(ga, "limit", 1);

        int offset = 0, limit = 0;

        if (s_off) {
            if (!parse_nonneg_int(s_off, &offset)) {
                VLOG(1, "  ERROR: --offset must be a non-negative integer, got '%s'", s_off);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--offset must be a non-negative integer\"}\n");
                usage_list(stderr);
                return EXIT_INVALID;
            }
        }
        if (s_lim) {
            if (!parse_nonneg_int(s_lim, &limit)) {
                VLOG(1, "  ERROR: --limit must be a non-negative integer, got '%s'", s_lim);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--limit must be a non-negative integer\"}\n");
                usage_list(stderr);
                return EXIT_INVALID;
            }
        }

        VLOG(1, "model_revision list: model_id=%d offset=%d limit=%d",
             model_id, offset, limit);

        VLOG(2, "  full: model_id=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             model_id, offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  model_id=%d offset=%d limit=%d", model_id, offset, limit);

        if (gopts->count) {
            int err = 0;
            int n = acta_db_model_revision_count(db, model_id, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "model_revision count");
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        model_revision_t **items = acta_db_model_revision_list_by_model(
            db, model_id, offset, limit, &out_count, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_model_revision_list_free(items, out_count);
            return finish_op_error(db, err, "model_revision list");
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_rev_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            rev_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                rev_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                rev_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_model_revision_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count <model_id> ─────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *model_id_str = cmd_args_next_positional(ga);
        if (!model_id_str) {
            VLOG(1, "model_revision count: ERROR missing <model_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <model_id>\"}\n");
            usage_count(stderr);
            return EXIT_INVALID;
        }
        int model_id;
        if (!parse_positive_id(model_id_str, &model_id)) {
            VLOG(1, "model_revision count: invalid model_id=%s", model_id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <model_id>: must be a positive integer\"}\n");
            usage_count(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_revision count: model_id=%d", model_id);

        VLOG(2, "  model_id=%d", model_id);

        int err = 0;
        int n = acta_db_model_revision_count(db, model_id, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "model_revision count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("model_revision", action, "actagamma_db model_revision help",
                          model_revision_actions, REV_ACTIONS);
}
