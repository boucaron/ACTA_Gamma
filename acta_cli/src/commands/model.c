/* File: model.c */
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
 *   acta_cli model --help
 */
void model_usage(FILE *f)
{
    fputs(
"Usage: acta_cli model <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new model\n"
"  get       Fetch a model by id\n"
"  update    Update an existing model\n"
"  delete    Remove a model (soft delete)\n"
"  restore   Restore a deleted model\n"
"  move      Move a model to another folder\n"
"  list      List models\n"
"  count     Count models\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n"
"== create ===========================================================\n"
"  Create a new model entry.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli model create \\\n"
"      --name \"My Model\" \\\n"
"      --backend openai \\\n"
"      --model_identifier gpt-4o \\\n"
"      --description \"Primary LLM\" \\\n"
"      --base_url https://api.openai.com/v1 \\\n"
"      --folder_id 3\n"
"        <- flag-based\n"
"\n"
"    cat model.json | acta_cli model create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>                 Display name\n"
"    --backend <str>              Backend identifier\n"
"    --model_identifier <str>     Upstream model id\n"
"\n"
"  Optional fields:\n"
"    --folder_id <int>            Owning folder (0 = root)\n"
"    --description <str>          Human-readable detail\n"
"    --base_url <str>             API base URL\n"
"    --configuration <json>       Arbitrary JSON config\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the entry as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single model by its primary key.\n"
"\n"
"    acta_cli model get 42\n"
"    acta_cli model get 42 --include_deleted\n"
"\n"
"  Options:\n"
"    --include_deleted    Return the row even if soft-deleted\n"
"    --deleted            Alias for --include_deleted\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== update <id> ====================================================\n"
"  Update one or more fields on an existing model.\n"
"\n"
"    acta_cli model update 42 --description \"New desc\"\n"
"    acta_cli model update 42 --base_url https://new.host/v1 \\\n"
"      --configuration '{\"timeout\":30}'\n"
"\n"
"  At least one field is required.  Unspecified fields are\n"
"  left unchanged.\n"
"\n"
"  Fields:\n"
"    --name <str>                 Display name\n"
"    --folder_id <int>            Owning folder\n"
"    --description <str>          Human-readable detail\n"
"    --backend <str>              Backend identifier\n"
"    --base_url <str>             API base URL\n"
"    --model_identifier <str>     Upstream model id\n"
"    --configuration <json>       Arbitrary JSON config\n"
"\n"
"== delete <id> ====================================================\n"
"  Soft-delete a model (sets deleted_at).\n"
"\n"
"    acta_cli model delete 42\n"
"\n"
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted model.\n"
"\n"
"    acta_cli model restore 42\n"
"\n"
"== move <id> ======================================================\n"
"  Move a model to a different folder.\n"
"\n"
"    acta_cli model move 42 --folder_id 7\n"
"    acta_cli model move 42 --folder_id 0    # root\n"
"\n"
"  Required:\n"
"    --folder_id <int>          Destination folder (0 = root)\n"
"\n"
"== list ===========================================================\n"
"  List models, optionally filtered by folder.\n"
"\n"
"    acta_cli model list\n"
"    acta_cli model list --folder_id 3 --offset 10 --limit 25\n"
"    acta_cli model list --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Filter by folder (omit = all)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count ==========================================================\n"
"  Count models, optionally filtered by folder.\n"
"\n"
"    acta_cli model count\n"
"    acta_cli model count --folder_id 3\n"
"    acta_cli model count --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Filter by folder (omit = all)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
"\n"
"Global options:\n"
"  --table            columnar / plain output instead of JSON\n"
"  --verbose <n>      debug level 0-3 (diagnostics on stderr)\n"
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
"  Create a new model entry.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli model create \\\n"
"      --name \"My Model\" \\\n"
"      --backend openai \\\n"
"      --model_identifier gpt-4o \\\n"
"      --description \"Primary LLM\" \\\n"
"      --base_url https://api.openai.com/v1 \\\n"
"      --folder_id 3\n"
"        <- flag-based\n"
"\n"
"    cat model.json | acta_cli model create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>                 Display name\n"
"    --backend <str>              Backend identifier\n"
"    --model_identifier <str>     Upstream model id\n"
"\n"
"  Optional fields:\n"
"    --folder_id <int>            Owning folder (0 = root)\n"
"    --description <str>          Human-readable detail\n"
"    --base_url <str>             API base URL\n"
"    --configuration <json>       Arbitrary JSON config\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the entry as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single model by its primary key.\n"
"\n"
"    acta_cli model get 42\n"
"    acta_cli model get 42 --include_deleted\n"
"\n"
"  Options:\n"
"    --include_deleted    Return the row even if soft-deleted\n"
"    --deleted            Alias for --include_deleted\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_update(FILE *f)
{
    fputs(
"== update <id> ====================================================\n"
"  Update one or more fields on an existing model.\n"
"\n"
"    acta_cli model update 42 --description \"New desc\"\n"
"    acta_cli model update 42 --base_url https://new.host/v1 \\\n"
"      --configuration '{\"timeout\":30}'\n"
"\n"
"  At least one field is required.  Unspecified fields are\n"
"  left unchanged.\n"
"\n"
"  Fields:\n"
"    --name <str>                 Display name\n"
"    --folder_id <int>            Owning folder\n"
"    --description <str>          Human-readable detail\n"
"    --backend <str>              Backend identifier\n"
"    --base_url <str>             API base URL\n"
"    --model_identifier <str>     Upstream model id\n"
"    --configuration <json>       Arbitrary JSON config\n", f);
}

static void usage_delete(FILE *f)
{
    fputs(
"== delete <id> ====================================================\n"
"  Soft-delete a model (sets deleted_at).\n"
"\n"
"    acta_cli model delete 42\n", f);
}

static void usage_restore(FILE *f)
{
    fputs(
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted model.\n"
"\n"
"    acta_cli model restore 42\n", f);
}

static void usage_move(FILE *f)
{
    fputs(
"== move <id> ======================================================\n"
"  Move a model to a different folder.\n"
"\n"
"    acta_cli model move 42 --folder_id 7\n"
"    acta_cli model move 42 --folder_id 0    # root\n"
"\n"
"  Required:\n"
"    --folder_id <int>          Destination folder (0 = root)\n", f);
}

static void usage_list(FILE *f)
{
    fputs(
"== list ===========================================================\n"
"  List models, optionally filtered by folder.\n"
"\n"
"    acta_cli model list\n"
"    acta_cli model list --folder_id 3 --offset 10 --limit 25\n"
"    acta_cli model list --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Filter by folder (omit = all)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
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
"== count ==========================================================\n"
"  Count models, optionally filtered by folder.\n"
"\n"
"    acta_cli model count\n"
"    acta_cli model count --folder_id 3\n"
"    acta_cli model count --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Filter by folder (omit = all)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_model_fields(const char *tag, const model_t *m)
{
    VLOG(2, "%s: id=%d folder_id=%d name=%s description=%s backend=%s "
         "base_url=%s model_identifier=%s configuration=%s "
         "created_at=%s updated_at=%s deleted_at=%s",
         tag,
         m->id,
         m->folder_id,
         m->name             ? m->name             : "(null)",
         m->description      ? m->description      : "(null)",
         m->backend          ? m->backend          : "(null)",
         m->base_url         ? m->base_url         : "(null)",
         m->model_identifier ? m->model_identifier : "(null)",
         m->configuration    ? m->configuration    : "(null)",
         m->created_at       ? m->created_at       : "(null)",
         m->updated_at       ? m->updated_at       : "(null)",
         m->deleted_at       ? m->deleted_at       : "(null)");
}

static void vlog_model_raw(const char *tag, const model_t *m, int rc)
{
    VLOG(3, "%s: m=%p id=%d rc=%d",
         tag, (const void *)m, m ? m->id : -1, rc);
}

/* free_row adapter for load_row_or_notfound (void* signature). */
static void model_free_wrap(void *m)
{
    acta_db_model_free((model_t *)m);
}

/* ── model_t → JSON object ────────────────────────────────────────── */

static void model_to_json(FILE *f, const model_t *m, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", m->id);
    }
    if ((!fl || fields_has(fl, "folder_id")) && !(gopts->no_nulls && m->folder_id == 0)) {
        if (shown++) fputs(", ", f);
        if (m->folder_id == 0)
            fputs("\"folder_id\":null", f);
        else
            fprintf(f, "\"folder_id\":%d", m->folder_id);
    }
    if ((!fl || fields_has(fl, "name")) && !(gopts->no_nulls && !m->name)) {
        if (shown++) fputs(", ", f);
        fputs("\"name\":", f);
        if (m->name) json_str(f, m->name); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "description")) && !(gopts->no_nulls && !m->description)) {
        if (shown++) fputs(", ", f);
        fputs("\"description\":", f);
        if (m->description) json_str(f, m->description); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "backend")) && !(gopts->no_nulls && !m->backend)) {
        if (shown++) fputs(", ", f);
        fputs("\"backend\":", f);
        if (m->backend) json_str(f, m->backend); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "base_url")) && !(gopts->no_nulls && !m->base_url)) {
        if (shown++) fputs(", ", f);
        fputs("\"base_url\":", f);
        if (m->base_url) json_str(f, m->base_url); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "model_identifier")) && !(gopts->no_nulls && !m->model_identifier)) {
        if (shown++) fputs(", ", f);
        fputs("\"model_identifier\":", f);
        if (m->model_identifier) json_str(f, m->model_identifier); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "configuration")) && !(gopts->no_nulls && !m->configuration)) {
        if (shown++) fputs(", ", f);
        fputs("\"configuration\":", f);
        if (m->configuration) json_str(f, m->configuration); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !m->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (m->created_at) json_str(f, m->created_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "updated_at")) && !(gopts->no_nulls && !m->updated_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"updated_at\":", f);
        if (m->updated_at) json_str(f, m->updated_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "deleted_at")) && !(gopts->no_nulls && !m->deleted_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"deleted_at\":", f);
        if (m->deleted_at) json_str(f, m->deleted_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void model_table(FILE *f, const model_t *m, int header)
{
    if (header) {
        fprintf(f, " %4s  %8s  %-20s  %-20s  %-10s  %-20s  %-30s  %-19s  %-19s\n",
                "ID", "FOLDER", "NAME", "DESCRIPTION", "BACKEND",
                "BASE_URL", "MODEL_IDENTIFIER", "CREATED_AT", "DELETED_AT");
        return;
    }
    char idb[16];
    char fdb[16];
    snprintf(idb, sizeof idb, "%d", m->id);
    snprintf(fdb, sizeof fdb, "%d", m->folder_id);
    fprintf(f, " %4s  ", idb);
    fprintf(f, " %8s  ", fdb);
    tcol(f, m->name,             20);
    tcol(f, m->description,      20);
    tcol(f, m->backend,          10);
    tcol(f, m->base_url,        20);
    tcol(f, m->model_identifier,30);
    tcol(f, m->created_at,      19);
    tcol(f, m->deleted_at,      19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t model_actions[] = {
    { "create",  "create a new model"              },
    { "get",     "fetch a model by id"             },
    { "update",  "update an existing model"        },
    { "delete",  "remove a model"                  },
    { "restore", "restore a deleted model"         },
    { "move",    "move a model to another folder"  },
    { "list",    "list all models"                 },
    { "count",   "count models"                    },
    { "help",    "show this help"                  },
};
#define MODEL_ACTIONS (sizeof(model_actions) / sizeof(model_actions[0]))

/* P0: print the help section for one model action.
 * 0 = printed, -1 = unknown action. */
int model_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "create")  == 0) usage_create(out);
    else if (strcmp(action, "get")     == 0) usage_get(out);
    else if (strcmp(action, "update")  == 0) usage_update(out);
    else if (strcmp(action, "delete")  == 0) usage_delete(out);
    else if (strcmp(action, "restore") == 0) usage_restore(out);
    else if (strcmp(action, "move")    == 0) usage_move(out);
    else if (strcmp(action, "list")    == 0) usage_list(out);
    else if (strcmp(action, "count")   == 0) usage_count(out);
    else return -1;
    return 0;
}

int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    /* ── help: whole entity, or one action via `model help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (model_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("model", sub, "acta_cli model help",
                                  model_actions, MODEL_ACTIONS);
        }
        model_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
        if (strcmp(action, "create") == 0) {
        model_t m = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_create(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "model create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_model(blob, &m) != 0) {
                VLOG(1, "  JSON parse error");
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"invalid JSON body\"}\n");
                usage_create(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            m.name             = (char *)cmd_args_flag(ga, "name", 1);
            m.description      = (char *)cmd_args_flag(ga, "description", 1);
            m.backend          = (char *)cmd_args_flag(ga, "backend", 1);
            m.base_url         = (char *)cmd_args_flag(ga, "base_url", 1);
            m.model_identifier = (char *)cmd_args_flag(ga, "model_identifier", 1);
            m.configuration    = (char *)cmd_args_flag(ga, "configuration", 1);

            /* --- folder_id: validate (atom) --- */
            m.folder_id = 0;  /* default: root (NULL) */
            if (parse_nonneg_int_flag(ga, "folder_id", &m.folder_id, 0,
                                      usage_create, "model create") < 0) {
                ret = EXIT_INVALID;
                goto cleanup_create;
            }
        }

        VLOG(1, "model create: name=%s backend=%s model_identifier=%s",
             m.name ? m.name : "(missing)",
             m.backend ? m.backend : "(missing)",
             m.model_identifier ? m.model_identifier : "(missing)");

        VLOG(2, "  params: name=%s folder_id=%d description=%s backend=%s "
                "base_url=%s model_identifier=%s configuration=%s",
             m.name             ? m.name             : "(null)",
             m.folder_id,
             m.description      ? m.description      : "(null)",
             m.backend          ? m.backend          : "(null)",
             m.base_url         ? m.base_url         : "(null)",
             m.model_identifier ? m.model_identifier : "(null)",
             m.configuration    ? m.configuration    : "(null)");

        VLOG(3, "  m=%p json_owned=%d", (const void *)&m, json_owned);

        if (!m.name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            emit_error("missing required field: name");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (m.name && strlen(m.name) == 0) {
            VLOG(1, "  ERROR: 'name' must not be empty");
            emit_error("field 'name' must not be empty");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (!m.backend) {
            VLOG(1, "  ERROR: missing required field 'backend'");
            emit_error("missing required field: backend");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }
        if (!m.model_identifier) {
            VLOG(1, "  ERROR: missing required field 'model_identifier'");
            emit_error("missing required field: model_identifier");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_create;
        }

        vlog_model_fields("  pre-create", &m);

        int out_id = 0;
        int rc = acta_db_model_create(db, &m, &out_id);
        VLOG(3, "  acta_db_model_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = finish_op_error(db, rc, "model create");
            goto cleanup_create;
        }

        VLOG(1, "  created model id=%d", out_id);
        emit_ok_id(gopts, out_id);

        ret = EXIT_OK;
        goto cleanup_create;

    cleanup_create:
        if (json_owned) {
            free(m.name);
            free(m.description);
            free(m.backend);
            free(m.base_url);
            free(m.model_identifier);
            free(m.configuration);
        }
        return ret;
    }



    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_get, "model get", &id))
            return EXIT_INVALID;

        int  include_deleted = cmd_args_has_flag(ga, "include_deleted");

        VLOG(1, "model get: id=%d include_deleted=%d fields=%s no_nulls=%d",
             id, include_deleted,
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls);

        /* ── fetch ────────────────────────────────────────────────── */
        int err = 0;
        /* include_deleted → unfiltered fetch (row even if soft-deleted);
         * otherwise live rows only (get_live returns NULL for deleted). */
        model_t *m = include_deleted
            ? acta_db_model_get(db, id, &err)
            : acta_db_model_get_live(db, id, &err);

        VLOG(3, "  fetch(id=%d, include_deleted=%d) → ptr=%p err=%d",
             id, include_deleted, (const void *)m, err);

        int rc = load_row_or_notfound(db, err, m, id, model_free_wrap,
                                      "model get", "model");
        if (rc)
            return rc;

        vlog_model_fields("  result", m);
        vlog_model_raw("  raw", m, 0);

        /* ── output ───────────────────────────────────────────────── */
        if (gopts->id_only) {
            /* --id_only: bare integer, ignores fields/no_nulls/table */
            fprintf(stdout, "%d\n", m->id);

        } else if (gopts->table) {
            /* --table: columnar; --fields restricts which columns */
            model_table(stdout, NULL, 1);           /* header row   */
            model_table(stdout, m, 0);             /* data row     */
            /* model_table() honours gopts->fields internally:
               it only prints the requested columns.              */

        } else {
            /* default: JSON object on one line
               gopts->fields    → only emit listed keys
               gopts->no_nulls  → skip keys whose value is NULL   */
            model_to_json(stdout, m, gopts);
            fputc('\n', stdout);
        }

        acta_db_model_free(m);
        return EXIT_OK;
    }


    /* ── update <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "update") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_update, "model update", &id))
            return EXIT_INVALID;

        /* ── input flags (flag mode; JSON mode: --json/--stdin/--from_file) ── */
        const char *f_name        = NULL;
        const char *f_description = NULL;
        const char *f_backend     = NULL;
        const char *f_base_url    = NULL;
        const char *f_model_ident = NULL;
        const char *f_config      = NULL;
        int has_folder = 0;   /* 1 once we've decided folder_id is being set */
        int  folder_val = 0;  /* 0 == root (NULL) */

        /* --name: present → non-empty (atom emits the empty-value error).
         * --folder_id: "0" → root (NULL), junk → EXIT_INVALID (atom). */
        if (require_flag(ga, "name", &f_name, usage_update,
                         "model update") < 0)
            return EXIT_INVALID;
        {
            int r_folder = parse_nonneg_int_flag(ga, "folder_id", &folder_val,
                                                 0, usage_update,
                                                 "model update");
            if (r_folder < 0)
                return EXIT_INVALID;
            has_folder = r_folder;
        }
        f_description = cmd_args_flag(ga, "description", 1);
        f_backend     = cmd_args_flag(ga, "backend", 1);
        f_base_url    = cmd_args_flag(ga, "base_url", 1);
        f_model_ident = cmd_args_flag(ga, "model_identifier", 1);
        f_config      = cmd_args_flag(ga, "configuration", 1);

        /* At least one field must be provided for update. */
        if (!f_name && !f_description && !f_backend && !f_base_url &&
            !f_model_ident && !f_config && !has_folder) {
            VLOG(1, "  ERROR: no fields provided for update");
            emit_error("at least one field required for update");
            usage_update(stderr);
            return EXIT_INVALID;
        }

        {
            char folder_str[16];
            const char *folder_disp;
            if (!has_folder)              folder_disp = "(unchanged)";
            else if (folder_val == 0)     folder_disp = "root";
            else {
                snprintf(folder_str, sizeof folder_str, "%d", folder_val);
                folder_disp = folder_str;
            }

            VLOG(1, "model update: id=%d name=%s folder_id=%s backend=%s "
                    "model_identifier=%s",
                 id,
                 f_name        ? f_name        : "(unchanged)",
                 folder_disp,
                 f_backend     ? f_backend     : "(unchanged)",
                 f_model_ident ? f_model_ident : "(unchanged)");
        }

        VLOG(2, "  params: name=%s folder_id=%d description=%s backend=%s "
                "base_url=%s model_identifier=%s configuration=%s",
             f_name        ? f_name        : "(null)",
             has_folder    ? folder_val    : -1,
             f_description ? f_description : "(null)",
             f_backend     ? f_backend     : "(null)",
             f_base_url    ? f_base_url    : "(null)",
             f_model_ident ? f_model_ident : "(null)",
             f_config      ? f_config      : "(null)");

        /*
         * Fetch the current row so we can fill in any fields the caller
         * did not supply (partial-update → full-update merge).
         */
        int err = 0;
        model_t *cur = acta_db_model_get_live(db, id, &err);
        {
            int rc_check = load_row_or_notfound(db, err, cur, id,
                                                model_free_wrap,
                                                "model update", "model");
            if (rc_check)
                return rc_check;
        }

        /* Shallow-merge: start from the live row, override only what
         * the caller actually passed.  All string pointers either point
         * into `cur` (freed together) or into the caller's arg buffers
         * (still alive until after the update call). */
        model_t m = *cur;

        if (f_name)
            m.name = (char *)f_name;
        if (has_folder)
            m.folder_id = folder_val;   /* 0 → NULL in the DB layer */
        if (f_description)
            m.description = (char *)f_description;
        if (f_backend)
            m.backend = (char *)f_backend;
        if (f_base_url)
            m.base_url = (char *)f_base_url;
        if (f_model_ident)
            m.model_identifier = (char *)f_model_ident;
        if (f_config)
            m.configuration = (char *)f_config;

        vlog_model_fields("  update-target", &m);

        int rc = acta_db_model_update(db, &m);

        VLOG(3, "  acta_db_model_update → rc=%d", rc);

        /* m shares string pointers with cur; free cur once (and m). */
        acta_db_model_free(cur);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model update");
        }

        VLOG(1, "  updated model id=%d", id);
        emit_ok_id(gopts, id);
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_delete, "model delete", &id))
            return EXIT_INVALID;

        VLOG(1, "model delete: id=%d", id);
        VLOG(3, "  id=%d db=%p", id, (const void *)db);

        int rc = acta_db_model_soft_delete(db, id);

        VLOG(3, "  acta_db_model_soft_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model delete");
        }

        VLOG(1, "  deleted model id=%d", id);
        emit_deleted();
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_restore, "model restore", &id))
            return EXIT_INVALID;

        VLOG(1, "model restore: id=%d", id);
        VLOG(3, "  id=%d db=%p", id, (const void *)db);

        int rc = acta_db_model_restore(db, id);

        VLOG(3, "  acta_db_model_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model restore");
        }

        VLOG(1, "  restored model id=%d", id);
        emit_ok_restored(gopts, id);
        return EXIT_OK;
    }


    /* ── move <id> --folder_id <fid> ──────────────────────────────── */
    if (strcmp(action, "move") == 0) {
        int model_id;
        if (!parse_id_positional(ga, "id", usage_move, "model move", &model_id))
            return EXIT_INVALID;

        int folder_id = 0;   /* 0 == root (NULL) */
        if (parse_nonneg_int_flag(ga, "folder_id", &folder_id, 1,
                                  usage_move, "model move") < 0)
            return EXIT_INVALID;

        VLOG(1, "model move: id=%d → folder_id=%d", model_id, folder_id);
        VLOG(2, "  folder_id=%d", folder_id);
        VLOG(3, "  model_id=%d folder_id=%d db=%p",
             model_id, folder_id, (const void *)db);

        int rc = acta_db_model_move_to_folder(db, model_id, folder_id);

        VLOG(3, "  acta_db_model_move_to_folder(%d, %d) → rc=%d",
             model_id, folder_id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "model move");
        }

        VLOG(1, "  moved model id=%d → folder_id=%d", model_id, folder_id);
        emit_ok_folder(gopts, model_id, folder_id);
        return EXIT_OK;
    }


    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        int folder_id = -1;  /* -1 = all */
        int offset = 0, limit = 0;
        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        if (parse_nonneg_int_flag(ga, "folder_id", &folder_id, 0,
                                  usage_list, "model list") < 0)
            return EXIT_INVALID;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_list, "model list") < 0)
            return EXIT_INVALID;

        VLOG(1, "model list: folder_id=%d offset=%d limit=%d include_deleted=%d",
             folder_id, offset, limit, include_deleted);

        VLOG(2, "  full: folder_id=%d offset=%d limit=%d include_deleted=%d "
                "no_nulls=%d table=%d fields=%s",
             folder_id, offset, limit, include_deleted,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  folder_id=%d offset=%d limit=%d include_deleted=%d db=%p",
             folder_id, offset, limit, include_deleted, (const void *)db);

        /* ── --count short-circuit ── */
        if (gopts->count) {
            int err = 0;
            int n;
            if (folder_id >= 0)
                n = include_deleted
                    ? acta_db_model_count_in_folder_with_deleted(
                          db, folder_id, &err)
                    : acta_db_model_count_in_folder(db, folder_id, &err);
            else
                n = include_deleted
                    ? acta_db_model_count_all_with_deleted(db, &err)
                    : acta_db_model_count_all(db, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "model count");
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
                model_usage(stderr);
                return EXIT_INVALID;
            }
            int emitted = 0;
            for (;;) {
                int want = (limit > 0) ? limit - emitted : 0;
                int o = offset + emitted;
                int n = 0, e2 = 0;
                model_t **items;
                if (folder_id >= 0)
                    items = include_deleted
                        ? acta_db_model_list_in_folder_with_deleted(
                              db, folder_id, o, want, &n, &e2)
                        : acta_db_model_list_in_folder(db, folder_id,
                                                       o, want, &n, &e2);
                else
                    items = include_deleted
                        ? acta_db_model_list_all_with_deleted(db, o, want,
                                                              &n, &e2)
                        : acta_db_model_list_all(db, o, want, &n, &e2);
                if (e2 != ACTA_DB_OK) {
                    acta_db_model_list_free(items, n);
                    return finish_op_error(db, e2, "model list");
                }
                for (int i = 0; i < n; i++) {
                    model_to_json(stdout, items[i], gopts);
                    fputc('\n', stdout);
                }
                acta_db_model_list_free(items, n);
                emitted += n;
                if ((limit > 0 && emitted >= limit) || n == 0 || n < want)
                    break;
            }
            VLOG(1, "  stream: %d item(s) emitted", emitted);
            return EXIT_OK;
        }

        /* ── normal list ── */
        int out_count = 0, err = 0;
        model_t **items;

        if (folder_id >= 0)
            items = include_deleted
                ? acta_db_model_list_in_folder_with_deleted(
                      db, folder_id, offset, limit, &out_count, &err)
                : acta_db_model_list_in_folder(db, folder_id,
                                               offset, limit,
                                               &out_count, &err);
        else
            items = include_deleted
                ? acta_db_model_list_all_with_deleted(
                      db, offset, limit, &out_count, &err)
                : acta_db_model_list_all(db,
                                         offset, limit,
                                         &out_count, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_model_list_free(items, out_count);
            return finish_op_error(db, err, "model list");
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_model_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            model_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                model_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                model_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_model_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count ────────────────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        int folder_id = -1;  /* -1 = all */
        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        if (parse_nonneg_int_flag(ga, "folder_id", &folder_id, 0,
                                  usage_count, "model count") < 0)
            return EXIT_INVALID;

        VLOG(1, "model count: folder_id=%d include_deleted=%d",
             folder_id, include_deleted);
        VLOG(3, "  folder_id=%d include_deleted=%d db=%p",
             folder_id, include_deleted, (const void *)db);

        int err = 0;
        int n;
        if (folder_id >= 0)
            n = include_deleted
                ? acta_db_model_count_in_folder_with_deleted(
                      db, folder_id, &err)
                : acta_db_model_count_in_folder(db, folder_id, &err);
        else
            n = include_deleted
                ? acta_db_model_count_all_with_deleted(db, &err)
                : acta_db_model_count_all(db, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "model count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("model", action, "acta_cli model help",
                          model_actions, MODEL_ACTIONS);
}
