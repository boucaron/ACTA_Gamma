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
 *   acta_cli model_revision --help
 */
void model_revision_usage(FILE *f)
{
    fputs(
"Usage: acta_cli model_revision <action> [options]\n"
"\n"
"Actions:\n"
"  get <id>         Fetch a model revision by id\n"
"  get-latest <model_id>  Fetch the latest revision for a model\n"
"  list <model_id>  List revisions for a model\n"
"  count <model_id> Count revisions for a model\n"
"  help <action>    Show help for a single action (no arg = full help)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single model revision by its primary key.\n"
"\n"
"    acta_cli model_revision get 42\n"
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
"    acta_cli model_revision get-latest 7\n"
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
"    acta_cli model_revision list 7\n"
"    acta_cli model_revision list 7 --offset 10 --limit 25\n"
"    acta_cli model_revision list 7 --include_deleted\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count <model_id> ================================================\n"
"  Count model revisions for a model.\n"
"\n"
"    acta_cli model_revision count 7\n"
"    acta_cli model_revision count 7 --include_deleted\n"
"\n"
"  Options:\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
"\n"
"Global options:\n"
"  --table              columnar / plain output instead of JSON\n"
"  --verbose [N]        debug level 0-3 (diagnostics on stderr)\n"
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
"    acta_cli model_revision get 42\n"
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
"    acta_cli model_revision get-latest 7\n"
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
"    acta_cli model_revision list 7\n"
"    acta_cli model_revision list 7 --offset 10 --limit 25\n"
"    acta_cli model_revision list 7 --include_deleted\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --stream             NDJSON: one JSON object per line; pages\n"
"                         internally until exhausted (P5)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_count(FILE *f)
{
    fputs(
"== count <model_id> ================================================\n"
"  Count model revisions for a model.\n"
"\n"
"    acta_cli model_revision count 7\n"
"    acta_cli model_revision count 7 --include_deleted\n"
"\n"
"  Options:\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n", f);
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

/* free_row adapter for load_row_or_notfound (void* signature). */
static void model_revision_free_wrap(void *r)
{
    acta_db_model_revision_free((model_revision_t *)r);
}

/* Pick the lister / counter variant from --include_deleted.
 * Default: live rows only (deleted_at IS NULL). */
static model_revision_t **rev_list_pick(db_t *db, int model_id,
                                        int include_deleted,
                                        int offset, int limit,
                                        int *out_count, int *err)
{
    return include_deleted
        ? acta_db_model_revision_list_by_model_with_deleted(
              db, model_id, offset, limit, out_count, err)
        : acta_db_model_revision_list_by_model(
              db, model_id, offset, limit, out_count, err);
}

static int rev_count_pick(db_t *db, int model_id,
                          int include_deleted, int *err)
{
    return include_deleted
        ? acta_db_model_revision_count_with_deleted(db, model_id, err)
        : acta_db_model_revision_count(db, model_id, err);
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

/* P0: print the help section for one model_revision action.
 * 0 = printed, -1 = unknown action. */
int model_revision_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "get")        == 0) usage_get(out);
    else if (strcmp(action, "get-latest") == 0) usage_get_latest(out);
    else if (strcmp(action, "list")     == 0) usage_list(out);
    else if (strcmp(action, "count")    == 0) usage_count(out);
    else return -1;
    return 0;
}

int cmd_model_revision(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                       db_t *db)
{
    /* ── help: whole entity, or one action via `model_revision help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (model_revision_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("model_revision", sub, "acta_cli model_revision help",
                                  model_revision_actions, REV_ACTIONS);
        }
        model_revision_usage(stdout);
        return EXIT_OK;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_get, "model_revision get", &id))
            return EXIT_INVALID;

        VLOG(1, "model_revision get: fetching id=%d", id);

        int err = 0;
        model_revision_t *r = acta_db_model_revision_get(db, id, &err);

        VLOG(3, "  acta_db_model_revision_get(%d) → ptr=%p err=%d",
             id, (const void *)r, err);

        int rc = load_row_or_notfound(db, err, r, id, model_revision_free_wrap,
                                      "model_revision get", "model_revision");
        if (rc)
            return rc;

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
        int model_id;
        if (!parse_id_positional(ga, "model_id", usage_get_latest,
                                 "model_revision get-latest", &model_id))
            return EXIT_INVALID;

        VLOG(1, "model_revision get-latest: fetching latest for model_id=%d", model_id);

        int err = 0;
        model_revision_t *r = acta_db_model_revision_get_latest(db, model_id, &err);

        VLOG(3, "  acta_db_model_revision_get_latest(%d) → ptr=%p err=%d",
             model_id, (const void *)r, err);

        int rc = load_row_or_notfound(db, err, r, model_id, model_revision_free_wrap,
                                      "model_revision latest", "model_revision");
        if (rc)
            return rc;

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
        /* KI-6: --id_only is a single-row modifier (get); list actions
         * reject it with exit 4 instead of silently ignoring it. */
        if (gopts->id_only) {
            emit_error("model_revision list: --id_only is not supported; "
                       "remove the flag (use the JSON rows, --count, "
                       "--table, or --stream)");
            return EXIT_INVALID;
        }

        int model_id;
        if (!parse_id_positional(ga, "model_id", usage_list,
                                 "model_revision list", &model_id))
            return EXIT_INVALID;

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_list, "model_revision list") < 0)
            return EXIT_INVALID;

        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        VLOG(1, "model_revision list: model_id=%d offset=%d limit=%d "
                "include_deleted=%d",
             model_id, offset, limit, include_deleted);

        VLOG(2, "  full: model_id=%d offset=%d limit=%d "
                "include_deleted=%d no_nulls=%d table=%d fields=%s",
             model_id, offset, limit, include_deleted,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  model_id=%d offset=%d limit=%d include_deleted=%d",
             model_id, offset, limit, include_deleted);

        if (gopts->count) {
            int err = 0;
            int n = rev_count_pick(db, model_id, include_deleted, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "model_revision count");
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
                model_revision_usage(stderr);
                return EXIT_INVALID;
            }
            int emitted = 0;
            for (;;) {
                int want = (limit > 0) ? limit - emitted : 0;
                int n = 0, e2 = 0;
                model_revision_t **items =
                    rev_list_pick(db, model_id, include_deleted,
                                  offset + emitted, want, &n, &e2);
                if (e2 != ACTA_DB_OK) {
                    acta_db_model_revision_list_free(items, n);
                    return finish_op_error(db, e2, "model_revision list");
                }
                for (int i = 0; i < n; i++) {
                    rev_to_json(stdout, items[i], gopts);
                    fputc('\n', stdout);
                }
                acta_db_model_revision_list_free(items, n);
                emitted += n;
                if ((limit > 0 && emitted >= limit) || n == 0 || n < want)
                    break;
            }
            VLOG(1, "  stream: %d item(s) emitted", emitted);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        model_revision_t **items = rev_list_pick(
            db, model_id, include_deleted, offset, limit, &out_count, &err);

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
        int model_id;
        if (!parse_id_positional(ga, "model_id", usage_count,
                                 "model_revision count", &model_id))
            return EXIT_INVALID;

        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        VLOG(1, "model_revision count: model_id=%d include_deleted=%d",
             model_id, include_deleted);

        VLOG(2, "  model_id=%d include_deleted=%d",
             model_id, include_deleted);

        int err = 0;
        int n = rev_count_pick(db, model_id, include_deleted, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "model_revision count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("model_revision", action, "acta_cli model_revision help",
                          model_revision_actions, REV_ACTIONS);
}
