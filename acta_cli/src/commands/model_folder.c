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
 *   acta_cli model_folder --help                                   */
void model_folder_usage(FILE *f)
{
    fputs(
"Usage: acta_cli model_folder <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new model folder\n"
"  get       Fetch a model folder by id\n"
"  list      List model folders\n"
"  count     Count model folders\n"
"  rename    Rename a model folder\n"
"  delete    Soft-delete a model folder\n"
"  restore   Restore a soft-deleted model folder\n"
"  move      Move a model folder to a new parent\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n"
"== create ===========================================================\n"
"  Create a new model folder.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli model_folder create \\\n"
"      --name \"my-folder\" \\\n"
"      --parent_id 3\n"
"        <- flag-based\n"
"\n"
"    cat folder.json | acta_cli model_folder create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>           Folder name\n"
"\n"
"  Optional fields:\n"
"    --parent_id <int>      Parent model folder id (0/omitted = root)\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the folder as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose [N]        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single model folder by its primary key.\n"
"\n"
"    acta_cli model_folder get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== list ==============================================================\n"
"  List model folders, optionally filtered by parent.\n"
"\n"
"    acta_cli model_folder list\n"
"    acta_cli model_folder list --parent_id 3 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --parent_id <int>    Filter by parent (0 = root children only)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count ============================================================\n"
"  Count model folders, optionally filtered by parent.\n"
"\n"
"    acta_cli model_folder count\n"
"    acta_cli model_folder count --parent_id 3\n"
"\n"
"  Options:\n"
"    --parent_id <int>    Filter by parent (0 = root children only)\n"
"\n"
"== rename <id> ======================================================\n"
"  Rename a model folder.\n"
"\n"
"    acta_cli model_folder rename 7 --name \"new-name\"\n"
"\n"
"  Required fields:\n"
"    --name <str>         New folder name\n"
"\n"
"== delete <id> ======================================================\n"
"  Soft-delete a model folder (sets deleted_at timestamp).\n"
"\n"
"    acta_cli model_folder delete 7\n"
"\n"
"== restore <id> ====================================================\n"
"  Restore a soft-deleted model folder.\n"
"\n"
"    acta_cli model_folder restore 7\n"
"\n"
"== move <id> =======================================================\n"
"  Move a model folder to a new parent.\n"
"\n"
"    acta_cli model_folder move 7 --parent_id 3\n"
"    acta_cli model_folder move 7 --parent_id 0   # move to root\n"
"\n"
"  Required fields:\n"
"    --parent_id <int>    New parent model folder id (0 = root)\n"
"\n"
"Global options:\n"
"  --table            columnar / plain output instead of JSON\n"
"  --verbose [N]      debug level 0-3 (diagnostics on stderr)\n"
"  --fields <csv>     comma-separated field whitelist\n"
"  --no_nulls         omit null-valued fields from JSON output\n"
"  --id_only          print only the id (create / get; rejected on list\n"
"                         — exit 4)\n"
"\n", f);
}

/* ── per-action usage snippets (printed to stderr on arg errors) ──── */

static void usage_mf_create(FILE *f)
{
    fputs(
"== create ===========================================================\n"
"  Create a new model folder.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli model_folder create \\\n"
"      --name \"my-folder\" \\\n"
"      --parent_id 3\n"
"        <- flag-based\n"
"\n"
"    cat folder.json | acta_cli model_folder create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>           Folder name\n"
"\n"
"  Optional fields:\n"
"    --parent_id <int>      Parent model folder id (0/omitted = root)\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the folder as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose [N]        debug level 0-3 (stderr)\n", f);
}

static void usage_mf_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single model folder by its primary key.\n"
"\n"
"    acta_cli model_folder get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_mf_list(FILE *f)
{
    fputs(
"== list ==============================================================\n"
"  List model folders, optionally filtered by parent.\n"
"\n"
"    acta_cli model_folder list\n"
"    acta_cli model_folder list --parent_id 3 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --parent_id <int>    Filter by parent (0 = root children only)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --stream             NDJSON: one JSON object per line; pages\n"
"                         internally until exhausted (P5)\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_mf_count(FILE *f)
{
    fputs(
"== count ============================================================\n"
"  Count model folders, optionally filtered by parent.\n"
"\n"
"    acta_cli model_folder count\n"
"    acta_cli model_folder count --parent_id 3\n"
"\n"
"  Options:\n"
"    --parent_id <int>    Filter by parent (0 = root children only)\n", f);
}

static void usage_mf_rename(FILE *f)
{
    fputs(
"== rename <id> ======================================================\n"
"  Rename a model folder.\n"
"\n"
"    acta_cli model_folder rename 7 --name \"new-name\"\n"
"\n"
"  Required fields:\n"
"    --name <str>         New folder name\n", f);
}

static void usage_mf_delete(FILE *f)
{
    fputs(
"== delete <id> ======================================================\n"
"  Soft-delete a model folder (sets deleted_at timestamp).\n"
"\n"
"    acta_cli model_folder delete 7\n", f);
}

static void usage_mf_restore(FILE *f)
{
    fputs(
"== restore <id> ====================================================\n"
"  Restore a soft-deleted model folder.\n"
"\n"
"    acta_cli model_folder restore 7\n", f);
}

static void usage_mf_move(FILE *f)
{
    fputs(
"== move <id> =======================================================\n"
"  Move a model folder to a new parent.\n"
"\n"
"    acta_cli model_folder move 7 --parent_id 3\n"
"    acta_cli model_folder move 7 --parent_id 0   # move to root\n"
"\n"
"  Required fields:\n"
"    --parent_id <int>    New parent model folder id (0 = root)\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_mf_fields(const char *tag, const model_folder_t *f)
{
    VLOG(2, "%s: id=%d name=%s parent_id=%d created_at=%s updated_at=%s deleted_at=%s",
         tag,
         f->id,
         f->name        ? f->name        : "(null)",
         f->parent_id,
         f->created_at  ? f->created_at  : "(null)",
         f->updated_at  ? f->updated_at  : "(null)",
         f->deleted_at  ? f->deleted_at  : "(null)");
}

static void vlog_mf_raw(const char *tag, const model_folder_t *f, int rc)
{
    VLOG(3, "%s: mf=%p id=%d rc=%d",
         tag, (const void *)f, f ? f->id : -1, rc);
}

/* free_row adapter for load_row_or_notfound (void* signature). */
static void model_folder_free_wrap(void *m)
{
    acta_db_model_folder_free((model_folder_t *)m);
}

/* ── model_folder_t → JSON object ─────────────────────────────────── */

static void mf_to_json(FILE *f, const model_folder_t *c, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", c->id);
    }
    if ((!fl || fields_has(fl, "name")) && !(gopts->no_nulls && !c->name)) {
        if (shown++) fputs(", ", f);
        fputs("\"name\":", f);
        if (c->name) json_str(f, c->name); else fputs("null", f);
    }
    if (!fl || fields_has(fl, "parent_id")) {
        if (shown++) fputs(", ", f);
        if (c->parent_id == 0)
            fputs("\"parent_id\":null", f);
        else
            fprintf(f, "\"parent_id\":%d", c->parent_id);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !c->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (c->created_at) json_str(f, c->created_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "updated_at")) && !(gopts->no_nulls && !c->updated_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"updated_at\":", f);
        if (c->updated_at) json_str(f, c->updated_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "deleted_at")) && !(gopts->no_nulls && !c->deleted_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"deleted_at\":", f);
        if (c->deleted_at) json_str(f, c->deleted_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void mf_table(FILE *f, const model_folder_t *c, int header)
{
    if (header) {
        fprintf(f, " %4s  %-20s  %10s  %-19s  %-19s  %-19s\n",
                "ID", "NAME", "PARENT_ID", "CREATED_AT", "UPDATED_AT", "DELETED_AT");
        return;
    }
    char idb[16];
    snprintf(idb, sizeof idb, "%d", c->id);
    fprintf(f, " %4s  ", idb);
    tcol(f, c->name, 20);
    if (c->parent_id == 0) {
        fprintf(f, " %10s  ", "-");
    } else {
        char pb[16];
        snprintf(pb, sizeof pb, "%d", c->parent_id);
        fprintf(f, " %10s  ", pb);
    }
    tcol(f, c->created_at, 19);
    tcol(f, c->updated_at, 19);
    tcol(f, c->deleted_at, 19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t model_folder_actions[] = {
    { "create",  "create a new model folder"     },
    { "get",     "fetch a model folder by id"    },
    { "list",    "list model folders"            },
    { "count",   "count model folders"           },
    { "rename",  "rename a model folder"         },
    { "delete",  "soft-delete a model folder"    },
    { "restore", "restore a soft-deleted model folder" },
    { "move",    "move a model folder to a new parent" },
    { "help",    "show this help"                }
};
#define MF_ACTIONS (sizeof(model_folder_actions) / sizeof(model_folder_actions[0]))

/* P0: print the help section for one model_folder action.
 * 0 = printed, -1 = unknown action. */
int model_folder_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "create")  == 0) usage_mf_create(out);
    else if (strcmp(action, "get")     == 0) usage_mf_get(out);
    else if (strcmp(action, "list")    == 0) usage_mf_list(out);
    else if (strcmp(action, "count")   == 0) usage_mf_count(out);
    else if (strcmp(action, "rename")  == 0) usage_mf_rename(out);
    else if (strcmp(action, "delete")  == 0) usage_mf_delete(out);
    else if (strcmp(action, "restore") == 0) usage_mf_restore(out);
    else if (strcmp(action, "move")    == 0) usage_mf_move(out);
    else return -1;
    return 0;
}

int cmd_model_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                     db_t *db)
{
    /* ── help: whole entity, or one action via `model_folder help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (model_folder_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("model_folder", sub, "acta_cli model_folder help",
                                  model_folder_actions, MF_ACTIONS);
        }
        model_folder_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        model_folder_t mf = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_mf_create(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "model_folder create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_model_folder(blob, &mf) != 0) {
                VLOG(1, "  JSON parse error");
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"invalid JSON body\"}\n");
                usage_mf_create(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            mf.name = (char *)cmd_args_flag(ga, "name", 1);

            /* --- parent_id: validate (atom) --- */
            mf.parent_id = 0;  /* default: root */
            if (parse_nonneg_int_flag(ga, "parent_id", &mf.parent_id, 0,
                                      usage_mf_create,
                                      "model_folder create") < 0) {
                ret = EXIT_INVALID;
                goto cleanup_mf_create;
            }
        }

        VLOG(1, "model_folder create: name=%s parent_id=%d",
             mf.name ? mf.name : "(missing)",
             mf.parent_id);

        VLOG(2, "  params: name=%s parent_id=%d fields=%s no_nulls=%d id_only=%d table=%d",
             mf.name ? mf.name : "(null)",
             mf.parent_id,
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p json_owned=%d mf=%p name=%p",
             (const void *)ga, json_owned, (const void *)&mf,
             (const void *)mf.name);

        /* ── required-field validation ────────────────────────────── */
        if (!mf.name || mf.name[0] == '\0' ) {
            VLOG(1, "  ERROR: missing required field 'name'");
            emit_error("missing required field: name");
            usage_mf_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_mf_create;
        }

        VLOG(1, "  creating model folder name='%s' parent_id=%d",
             mf.name, mf.parent_id);

        int out_id = 0;
        int rc = acta_db_model_folder_create(db, mf.name, mf.parent_id, &out_id);

        VLOG(3, "  acta_db_model_folder_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = finish_op_error(db, rc, "model_folder create");
            goto cleanup_mf_create;
        }

        VLOG(1, "  created model folder id=%d", out_id);
        emit_ok_id(gopts, out_id);

        ret = EXIT_OK;
        goto cleanup_mf_create;

    cleanup_mf_create:
        if (json_owned) {
            free(mf.name);
            free(mf.created_at);
            free(mf.updated_at);
            free(mf.deleted_at);
        }
        return ret;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_mf_get,
                                 "model_folder get", &id))
            return EXIT_INVALID;

        VLOG(1, "model_folder get: fetching id=%d", id);

        int err = 0;
        model_folder_t *c = acta_db_model_folder_get(db, id, &err);

        VLOG(3, "  acta_db_model_folder_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        int rc = load_row_or_notfound(db, err, c, id,
                                      model_folder_free_wrap,
                                      "model_folder get", "model_folder");
        if (rc)
            return rc;

        vlog_mf_fields("  result", c);
        vlog_mf_raw("  raw", c, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", c->id);
        } else if (gopts->table) {
            mf_table(stdout, NULL, 1);
            mf_table(stdout, c, 0);
        } else {
            mf_to_json(stdout, c, gopts);
            fputc('\n', stdout);
        }
        acta_db_model_folder_free(c);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        /* KI-6: --id_only is a single-row modifier (create/get); list
         * actions reject it with exit 4 instead of silently ignoring it. */
        if (gopts->id_only) {
            emit_error("model_folder list: --id_only is not supported; "
                       "remove the flag (use the JSON rows, --count, "
                       "--table, or --stream)");
            return EXIT_INVALID;
        }

        const char *s_parent = cmd_args_flag(ga, "parent_id", 1);

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_mf_list, "model_folder list") < 0)
            return EXIT_INVALID;

        int parent_id = 0;  /* 0 = all (no filter) */
        int has_parent;
        if ((has_parent = parse_nonneg_int_flag(ga, "parent_id", &parent_id,
                                                0, usage_mf_list,
                                                "model_folder list")) < 0)
            return EXIT_INVALID;

        VLOG(1, "model_folder list: parent_id=%s offset=%d limit=%d",
             s_parent ? s_parent : "(all)",
             offset, limit);

        VLOG(2, "  full: parent_id=%d offset=%d limit=%d no_nulls=%d table=%d fields=%s",
             has_parent ? parent_id : -1,
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  has_parent=%d parent_id=%d offset=%d limit=%d",
             has_parent, parent_id, offset, limit);

        if (gopts->count) {
            int err = 0;
            int n;
            if (has_parent) {
                n = acta_db_model_folder_count_children(db, parent_id, &err);
            } else {
                n = acta_db_model_folder_count_all(db, &err);
            }
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "model_folder count");
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
                model_folder_usage(stderr);
                return EXIT_INVALID;
            }
            int emitted = 0;
            for (;;) {
                int want = (limit > 0) ? limit - emitted : 0;
                int o = offset + emitted;
                int n = 0, e2 = 0;
                model_folder_t **items = has_parent
                    ? acta_db_model_folder_list_children(db, parent_id,
                                                         o, want, &n, &e2)
                    : acta_db_model_folder_list_all(db, o, want, &n, &e2);
                if (e2 != ACTA_DB_OK) {
                    acta_db_model_folder_list_free(items, n);
                    return finish_op_error(db, e2, "model_folder list");
                }
                for (int i = 0; i < n; i++) {
                    mf_to_json(stdout, items[i], gopts);
                    fputc('\n', stdout);
                }
                acta_db_model_folder_list_free(items, n);
                emitted += n;
                if ((limit > 0 && emitted >= limit) || n == 0 || n < want)
                    break;
            }
            VLOG(1, "  stream: %d item(s) emitted", emitted);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        model_folder_t **items;

        if (has_parent) {
            items = acta_db_model_folder_list_children(db, parent_id,
                                                       offset, limit,
                                                       &out_count, &err);
        } else {
            items = acta_db_model_folder_list_all(db, offset, limit,
                                                  &out_count, &err);
        }

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_model_folder_list_free(items, out_count);
            return finish_op_error(db, err, "model_folder list");
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_mf_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            mf_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                mf_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                mf_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_model_folder_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count ────────────────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *s_parent = cmd_args_flag(ga, "parent_id", 1);

        int parent_id = 0;
        int has_parent;
        if ((has_parent = parse_nonneg_int_flag(ga, "parent_id", &parent_id,
                                                0, usage_mf_count,
                                                "model_folder count")) < 0)
            return EXIT_INVALID;

        VLOG(1, "model_folder count: parent_id=%s",
             s_parent ? s_parent : "(all)");

        VLOG(2, "  has_parent=%d parent_id=%d", has_parent, parent_id);

        int err = 0;
        int n;
        if (has_parent) {
            n = acta_db_model_folder_count_children(db, parent_id, &err);
        } else {
            n = acta_db_model_folder_count_all(db, &err);
        }
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "model_folder count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── rename <id> --name <new-name> ────────────────────────────── */
    if (strcmp(action, "rename") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_mf_rename,
                                 "model_folder rename", &id))
            return EXIT_INVALID;

        const char *new_name = NULL;
        if (require_flag(ga, "name", &new_name, usage_mf_rename,
                         "model_folder rename") < 0)
            return EXIT_INVALID;
        if (!new_name) {
            VLOG(1, "model_folder rename: ERROR missing required --name");
            emit_error("missing required field: name");
            usage_mf_rename(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_folder rename: id=%d new_name=%s", id, new_name);

        VLOG(2, "  id=%d name='%s' table=%d id_only=%d",
             id, new_name, gopts->table, gopts->id_only);

        int rc = acta_db_model_folder_rename(db, id, new_name);

        VLOG(3, "  acta_db_model_folder_rename(%d, '%s') → rc=%d", id, new_name, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model_folder rename");
        }

        VLOG(1, "  renamed model folder id=%d → '%s'", id, new_name);
        if (gopts->id_only)
            emit_ok_id(gopts, id);
        else {
            fprintf(stdout, "{\"id\":%d,\"name\":", id);
            json_str(stdout, new_name);
            fputs("}\n", stdout);
        }
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_mf_delete,
                                 "model_folder delete", &id))
            return EXIT_INVALID;

        VLOG(1, "model_folder delete: id=%d", id);

        int rc = acta_db_model_folder_soft_delete(db, id);

        VLOG(3, "  acta_db_model_folder_soft_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model_folder delete");
        }

        VLOG(1, "  soft-deleted model folder id=%d", id);
        emit_deleted();
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_mf_restore,
                                 "model_folder restore", &id))
            return EXIT_INVALID;

        VLOG(1, "model_folder restore: id=%d", id);

        int rc = acta_db_model_folder_restore(db, id);

        VLOG(3, "  acta_db_model_folder_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model_folder restore");
        }

        VLOG(1, "  restored model folder id=%d", id);
        emit_ok_restored(gopts, id);
        return EXIT_OK;
    }

    /* ── move <id> --parent_id <new-parent> ───────────────────────── */
    if (strcmp(action, "move") == 0) {
        int folder_id;
        if (!parse_id_positional(ga, "id", usage_mf_move,
                                 "model_folder move", &folder_id))
            return EXIT_INVALID;

        int new_parent_id = 0;
        if (parse_nonneg_int_flag(ga, "parent_id", &new_parent_id, 1,
                                  usage_mf_move,
                                  "model_folder move") < 0)
            return EXIT_INVALID;

        VLOG(1, "model_folder move: folder_id=%d new_parent_id=%d",
             folder_id, new_parent_id);

        VLOG(2, "  folder_id=%d new_parent_id=%d", folder_id, new_parent_id);

        int rc = acta_db_model_folder_move_to(db, folder_id, new_parent_id);

        VLOG(3, "  acta_db_model_folder_move_to(%d, %d) → rc=%d",
             folder_id, new_parent_id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model_folder move");
        }

        VLOG(1, "  moved model folder id=%d → parent_id=%d", folder_id, new_parent_id);
        emit_ok_parent(gopts, folder_id, new_parent_id);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("model_folder", action, "acta_cli model_folder help",
                          model_folder_actions, MF_ACTIONS);
}
