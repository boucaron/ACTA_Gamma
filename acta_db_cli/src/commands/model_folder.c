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
 */

static const global_opts_t *vlog_gopts;

#define VLOG(lvl, fmt, ...)                                              \
    do {                                                                 \
        if (vlog_gopts && vlog_gopts->verbose >= (lvl)) {                 \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);     \
        }                                                                \
    } while (0)

/* ══════════════════════════════════════════════════════════════════ */
/*  Usage / help                                                       */
/* ══════════════════════════════════════════════════════════════════ */

/* Non-static: the global dispatch layer can call this for
 *   actagamma_db model_folder --help                                   */
void model_folder_usage(FILE *f)
{
    fputs(
"Usage: actagamma_db model_folder <action> [options]\n"
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
"  help      Show this help\n"
"\n"
"== create ===========================================================\n"
"  Create a new model folder.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    actagamma_db model_folder create \\\n"
"      --name \"my-folder\" \\\n"
"      --parent_id 3\n"
"        <- flag-based\n"
"\n"
"    cat folder.json | actagamma_db model_folder create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>           Folder name\n"
"\n"
"  Optional fields:\n"
"    --parent_id <int>      Parent model folder id (0/omitted = root)\n"
"\n"
"  Options:\n"
"    --json               Read the folder as JSON from stdin\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single model folder by its primary key.\n"
"\n"
"    actagamma_db model_folder get 42\n"
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
"    actagamma_db model_folder list\n"
"    actagamma_db model_folder list --parent_id 3 --offset 10 --limit 25\n"
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
"    actagamma_db model_folder count\n"
"    actagamma_db model_folder count --parent_id 3\n"
"\n"
"  Options:\n"
"    --parent_id <int>    Filter by parent (0 = root children only)\n"
"\n"
"== rename <id> ======================================================\n"
"  Rename a model folder.\n"
"\n"
"    actagamma_db model_folder rename 7 --name \"new-name\"\n"
"\n"
"  Required fields:\n"
"    --name <str>         New folder name\n"
"\n"
"== delete <id> ======================================================\n"
"  Soft-delete a model folder (sets deleted_at timestamp).\n"
"\n"
"    actagamma_db model_folder delete 7\n"
"\n"
"== restore <id> ====================================================\n"
"  Restore a soft-deleted model folder.\n"
"\n"
"    actagamma_db model_folder restore 7\n"
"\n"
"== move <id> =======================================================\n"
"  Move a model folder to a new parent.\n"
"\n"
"    actagamma_db model_folder move 7 --parent_id 3\n"
"    actagamma_db model_folder move 7 --parent_id 0   # move to root\n"
"\n"
"  Required fields:\n"
"    --parent_id <int>    New parent model folder id (0 = root)\n"
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

static void usage_mf_create(FILE *f)
{
    fputs(
"== create ===========================================================\n"
"  Create a new model folder.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    actagamma_db model_folder create \\\n"
"      --name \"my-folder\" \\\n"
"      --parent_id 3\n"
"        <- flag-based\n"
"\n"
"    cat folder.json | actagamma_db model_folder create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>           Folder name\n"
"\n"
"  Optional fields:\n"
"    --parent_id <int>      Parent model folder id (0/omitted = root)\n"
"\n"
"  Options:\n"
"    --json               Read the folder as JSON from stdin\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_mf_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single model folder by its primary key.\n"
"\n"
"    actagamma_db model_folder get 42\n"
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
"    actagamma_db model_folder list\n"
"    actagamma_db model_folder list --parent_id 3 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --parent_id <int>    Filter by parent (0 = root children only)\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_mf_count(FILE *f)
{
    fputs(
"== count ============================================================\n"
"  Count model folders, optionally filtered by parent.\n"
"\n"
"    actagamma_db model_folder count\n"
"    actagamma_db model_folder count --parent_id 3\n"
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
"    actagamma_db model_folder rename 7 --name \"new-name\"\n"
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
"    actagamma_db model_folder delete 7\n", f);
}

static void usage_mf_restore(FILE *f)
{
    fputs(
"== restore <id> ====================================================\n"
"  Restore a soft-deleted model folder.\n"
"\n"
"    actagamma_db model_folder restore 7\n", f);
}

static void usage_mf_move(FILE *f)
{
    fputs(
"== move <id> =======================================================\n"
"  Move a model folder to a new parent.\n"
"\n"
"    actagamma_db model_folder move 7 --parent_id 3\n"
"    actagamma_db model_folder move 7 --parent_id 0   # move to root\n"
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

int cmd_model_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                     db_t *db)
{
    vlog_gopts = gopts;

    /* ── help (subcommand-level; only the bare word "help") ──────── */
    if (strcmp(action, "help") == 0) {
        model_folder_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        model_folder_t mf = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        if (gopts->json_input) {
            char *blob = read_stdin_all();
            if (!blob) {
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"failed to read JSON input\"}\n");
                usage_mf_create(stderr);
                return EXIT_INVALID;
            }
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
            const char *f_name     = cmd_args_flag(ga, "name", 1);
            const char *f_parent   = cmd_args_flag(ga, "parent_id", 1);

            mf.name      = (char *)f_name;
            mf.parent_id = f_parent ? atoi(f_parent) : 0;
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
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
            usage_mf_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_mf_create;
        }

        /* ── optional-field validation ────────────────────────────── */
        if (mf.parent_id < 0) {
            VLOG(1, "  ERROR: 'parent_id' must be non-negative, got %d",
                  mf.parent_id);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"parent_id must be non-negative\"}\n");
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
            ret = map_rc_to_exit(rc);
            goto cleanup_mf_create;
        }

        VLOG(1, "  created model folder id=%d", out_id);

        if (gopts->id_only)
            fprintf(stdout, "%d\n", out_id);
        else
            fprintf(stdout, "{\"id\":%d}\n", out_id);

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
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model_folder get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_mf_get(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model_folder get: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_mf_get(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_folder get: fetching id=%d", id);

        int err = 0;
        model_folder_t *c = acta_db_model_folder_get(db, id, &err);

        VLOG(3, "  acta_db_model_folder_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_model_folder_free(c);
            return map_rc_to_exit(err);
        }
        if (!c) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

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
        const char *s_parent = cmd_args_flag(ga, "parent_id", 1);
        const char *s_off    = cmd_args_flag(ga, "offset", 1);
        const char *s_lim    = cmd_args_flag(ga, "limit", 1);

        int offset = 0, limit = 0;

        if (s_off) {
            char *end;
            long v = strtol(s_off, &end, 10);
            if (*end || v < 0) {
                VLOG(1, "  ERROR: --offset must be a non-negative integer, got '%s'", s_off);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--offset must be a non-negative integer\"}\n");
                usage_mf_list(stderr);
                return EXIT_INVALID;
            }
            offset = (int)v;
        }
        if (s_lim) {
            char *end;
            long v = strtol(s_lim, &end, 10);
            if (*end || v < 0) {
                VLOG(1, "  ERROR: --limit must be a non-negative integer, got '%s'", s_lim);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--limit must be a non-negative integer\"}\n");
                usage_mf_list(stderr);
                return EXIT_INVALID;
            }
            limit = (int)v;
        }

        int parent_id = 0;  /* 0 = all (no filter) */
        int has_parent = 0;
        if (s_parent) {
            parent_id = atoi(s_parent);
            if (parent_id < 0) {
                VLOG(1, "  ERROR: --parent_id must be non-negative, got '%s'", s_parent);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--parent_id must be non-negative\"}\n");
                usage_mf_list(stderr);
                return EXIT_INVALID;
            }
            has_parent = 1;
        }

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
                return map_rc_to_exit(err);
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
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
            return map_rc_to_exit(err);
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
        int has_parent = 0;
        if (s_parent) {
            parent_id = atoi(s_parent);
            if (parent_id < 0) {
                VLOG(1, "  ERROR: --parent_id must be non-negative, got '%s'", s_parent);
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--parent_id must be non-negative\"}\n");
                usage_mf_count(stderr);
                return EXIT_INVALID;
            }
            has_parent = 1;
        }

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
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── rename <id> --name <new-name> ────────────────────────────── */
    if (strcmp(action, "rename") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model_folder rename: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_mf_rename(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model_folder rename: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_mf_rename(stderr);
            return EXIT_INVALID;
        }

        const char *new_name = cmd_args_flag(ga, "name", 1);
        if (!new_name) {
            VLOG(1, "model_folder rename: ERROR missing required --name");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
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
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  renamed model folder id=%d → '%s'", id, new_name);
        if (gopts->id_only)
            fprintf(stdout, "%d\n", id);
        else {
            fprintf(stdout, "{\"id\":%d,\"name\":", id);
            json_str(stdout, new_name);
            fputs("}\n", stdout);
        }
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model_folder delete: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_mf_delete(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model_folder delete: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_mf_delete(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_folder delete: id=%d", id);

        int rc = acta_db_model_folder_soft_delete(db, id);

        VLOG(3, "  acta_db_model_folder_soft_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  soft-deleted model folder id=%d", id);
        fprintf(stdout, "{\"deleted\":true}\n");
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model_folder restore: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_mf_restore(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model_folder restore: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_mf_restore(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_folder restore: id=%d", id);

        int rc = acta_db_model_folder_restore(db, id);

        VLOG(3, "  acta_db_model_folder_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  restored model folder id=%d", id);
        if (gopts->id_only)
            fprintf(stdout, "%d\n", id);
        else
            fprintf(stdout, "{\"id\":%d,\"restored\":true}\n", id);
        return EXIT_OK;
    }

    /* ── move <id> --parent_id <new-parent> ───────────────────────── */
    if (strcmp(action, "move") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model_folder move: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_mf_move(stderr);
            return EXIT_INVALID;
        }
        int folder_id = atoi(id_str);
        if (folder_id <= 0) {
            VLOG(1, "model_folder move: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_mf_move(stderr);
            return EXIT_INVALID;
        }

        const char *s_new_parent = cmd_args_flag(ga, "parent_id", 1);
        if (!s_new_parent) {
            VLOG(1, "model_folder move: ERROR missing required --parent_id");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: parent_id\"}\n");
            usage_mf_move(stderr);
            return EXIT_INVALID;
        }
        int new_parent_id = atoi(s_new_parent);
        if (new_parent_id < 0) {
            VLOG(1, "model_folder move: --parent_id must be non-negative, got '%s'", s_new_parent);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"--parent_id must be non-negative\"}\n");
            usage_mf_move(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "model_folder move: folder_id=%d new_parent_id=%d",
             folder_id, new_parent_id);

        VLOG(2, "  folder_id=%d new_parent_id=%d", folder_id, new_parent_id);

        int rc = acta_db_model_folder_move_to(db, folder_id, new_parent_id);

        VLOG(3, "  acta_db_model_folder_move_to(%d, %d) → rc=%d",
             folder_id, new_parent_id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  moved model folder id=%d → parent_id=%d", folder_id, new_parent_id);
        if (gopts->id_only)
            fprintf(stdout, "%d\n", folder_id);
        else
            fprintf(stdout, "{\"id\":%d,\"parent_id\":%s}\n",
                    folder_id,
                    new_parent_id == 0 ? "null" : s_new_parent);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    {
        const char *guess = closest_action(action, model_folder_actions, MF_ACTIONS);

        VLOG(1, "model_folder: unknown action '%s'%s",
             action ? action : "(null)",
             guess   ? "  (suggestion below)" : "");

        fprintf(stderr, "Unknown action '%s'.\n", action ? action : "(null)");
        if (guess)
            fprintf(stderr, "  Did you mean '%s'?\n", guess);
        fprintf(stderr, "  Run 'actagamma_db model_folder help' for full usage.\n");
        return EXIT_INVALID;
    }
}
