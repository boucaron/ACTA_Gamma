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
 *   acta_cli skill_folder --help                                     */
void skill_folder_usage(FILE *f)
{
    fputs(
"Usage: acta_cli skill_folder <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new skill_folder\n"
"  get       Fetch a skill_folder by id\n"
"  list      List skill_folders (all or by parent)\n"
"  count     Count skill_folders\n"
"  rename    Rename a skill_folder\n"
"  move      Move a skill_folder to a new parent\n"
"  delete    Soft-delete a skill_folder\n"
"  restore   Restore a soft-deleted skill_folder\n"
"  help      Show this help\n"
"\n"
"== create ===========================================================\n"
"  Create a new skill_folder.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli skill_folder create \\\n"
"      --name \"My Folder\" \\\n"
"      --parent_id 1\n"
"        <- flag-based\n"
"\n"
"    cat entry.json | acta_cli skill_folder create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>           Skill_folder name (must be non-empty)\n"
"\n"
"  Optional fields:\n"
"    --parent_id <int>      Parent skill_folder id (0 = root, default 0)\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the entry as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single skill_folder by its primary key.\n"
"\n"
"    acta_cli skill_folder get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== list [parent_id | all] ==========================================\n"
"  List skill_folders.  Omit the positional (or pass 'all') for all\n"
"  skill_folders; pass a numeric id to list children of that\n"
"  skill_folder.\n"
"\n"
"    acta_cli skill_folder list\n"
"    acta_cli skill_folder list 1\n"
"    acta_cli skill_folder list 1 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count [parent_id | all] =========================================\n"
"  Count skill_folders (all, or children of a given parent).\n"
"\n"
"    acta_cli skill_folder count\n"
"    acta_cli skill_folder count 1\n"
"\n"
"== rename <id> --name <new> ========================================\n"
"  Rename a skill_folder.\n"
"\n"
"    acta_cli skill_folder rename 42 --name \"New Name\"\n"
"\n"
"  Required flags:\n"
"    --name <str>         New skill_folder name\n"
"\n"
"== move <id> [--parent_id <pid>] ===================================\n"
"  Move a skill_folder under a different parent.\n"
"  Omit --parent_id (or pass 0) to move to root.\n"
"\n"
"    acta_cli skill_folder move 42 --parent_id 7\n"
"    acta_cli skill_folder move 42            # → root\n"
"\n"
"  Options:\n"
"    --parent_id <int>    New parent skill_folder id (default 0 = root)\n"
"\n"
"== delete <id> ====================================================\n"
"  Soft-delete a skill_folder (sets deleted_at).\n"
"\n"
"    acta_cli skill_folder delete 42\n"
"\n"
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted skill_folder.\n"
"\n"
"    acta_cli skill_folder restore 42\n"
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

static void usage_sf_create(FILE *f)
{
    fputs(
"== create ===========================================================\n"
"  Create a new skill_folder.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli skill_folder create \\\n"
"      --name \"My Folder\" \\\n"
"      --parent_id 1\n"
"        <- flag-based\n"
"\n"
"    cat entry.json | acta_cli skill_folder create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>           Skill_folder name (must be non-empty)\n"
"\n"
"  Optional fields:\n"
"    --parent_id <int>      Parent skill_folder id (0 = root, default 0)\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the entry as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_sf_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single skill_folder by its primary key.\n"
"\n"
"    acta_cli skill_folder get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_sf_list(FILE *f)
{
    fputs(
"== list [parent_id | all] ==========================================\n"
"  List skill_folders.  Omit the positional (or pass 'all') for all\n"
"  skill_folders; pass a numeric id to list children of that\n"
"  skill_folder.\n"
"\n"
"    acta_cli skill_folder list\n"
"    acta_cli skill_folder list 1\n"
"    acta_cli skill_folder list 1 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_sf_count(FILE *f)
{
    fputs(
"== count [parent_id | all] =========================================\n"
"  Count skill_folders (all, or children of a given parent).\n"
"\n"
"    acta_cli skill_folder count\n"
"    acta_cli skill_folder count 1\n", f);
}

static void usage_sf_rename(FILE *f)
{
    fputs(
"== rename <id> --name <new> ========================================\n"
"  Rename a skill_folder.\n"
"\n"
"    acta_cli skill_folder rename 42 --name \"New Name\"\n"
"\n"
"  Required flags:\n"
"    --name <str>         New skill_folder name\n", f);
}

static void usage_sf_move(FILE *f)
{
    fputs(
"== move <id> [--parent_id <pid>] ===================================\n"
"  Move a skill_folder under a different parent.\n"
"  Omit --parent_id (or pass 0) to move to root.\n"
"\n"
"    acta_cli skill_folder move 42 --parent_id 7\n"
"    acta_cli skill_folder move 42            # → root\n"
"\n"
"  Options:\n"
"    --parent_id <int>    New parent skill_folder id (default 0 = root)\n", f);
}

static void usage_sf_delete(FILE *f)
{
    fputs(
"== delete <id> ====================================================\n"
"  Soft-delete a skill_folder (sets deleted_at).\n"
"\n"
"    acta_cli skill_folder delete 42\n", f);
}

static void usage_sf_restore(FILE *f)
{
    fputs(
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted skill_folder.\n"
"\n"
"    acta_cli skill_folder restore 42\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_sf_fields(const char *tag, const skill_folder_t *f)
{
    VLOG(2, "%s: id=%d name=%s parent_id=%d created_at=%s updated_at=%s deleted_at=%s",
         tag,
         f->id,
         f->name         ? f->name         : "(null)",
         f->parent_id,
         f->created_at   ? f->created_at   : "(null)",
         f->updated_at   ? f->updated_at   : "(null)",
         f->deleted_at   ? f->deleted_at   : "(null)");
}

static void vlog_sf_raw(const char *tag, const skill_folder_t *f, int rc)
{
    VLOG(3, "%s: sf=%p id=%d rc=%d",
         tag, (const void *)f, f ? f->id : -1, rc);
}

/* free_row adapter for load_row_or_notfound (void* signature). */
static void sf_free_wrap(void *p)
{
    acta_db_skill_folder_free((skill_folder_t *)p);
}

/* ── skill_folder_t → JSON object ─────────────────────────────────── */

static void sf_to_json(FILE *f, const skill_folder_t *c, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
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

static void sf_table(FILE *f, const skill_folder_t *c, int header)
{
    if (header) {
        fprintf(f, " %4s  %-20s  %10s  %-19s  %-19s  %-19s\n",
                "ID", "NAME", "PARENT_ID", "CREATED_AT", "UPDATED_AT", "DELETED_AT");
        return;
    }
    char idb[16];
    char pidb[16];
    snprintf(idb, sizeof idb, "%d", c->id);
    if (c->parent_id == 0)
        snprintf(pidb, sizeof pidb, "-");
    else
        snprintf(pidb, sizeof pidb, "%d", c->parent_id);
    fprintf(f, " %4s  ", idb);
    tcol(f, c->name, 20);
    fprintf(f, " %10s  ", pidb);
    tcol(f, c->created_at, 19);
    tcol(f, c->updated_at, 19);
    tcol(f, c->deleted_at, 19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t skill_folder_actions[] = {
    { "create",  "create a new skill_folder"        },
    { "get",     "fetch a skill_folder by id"       },
    { "list",    "list skill_folders (all or by parent)" },
    { "count",   "count skill_folders"             },
    { "rename",  "rename a skill_folder"           },
    { "move",    "move a skill_folder to a new parent" },
    { "delete",  "soft-delete a skill_folder"      },
    { "restore", "restore a soft-deleted skill_folder" },
    { "help",    "show this help"                  },
};
#define SF_ACTIONS (sizeof(skill_folder_actions) / sizeof(skill_folder_actions[0]))

int cmd_skill_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                     db_t *db)
{
    /* ── help (subcommand-level; only the bare word "help") ──────── */
    if (strcmp(action, "help") == 0) {
        skill_folder_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        skill_folder_t sf = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_sf_create(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "skill_folder create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_skill_folder(blob, &sf) != 0) {
                VLOG(1, "  JSON parse error");
                emit_error("invalid JSON body");
                usage_sf_create(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            sf.name = (char *)cmd_args_flag(ga, "name", 1);

            if (parse_nonneg_int_flag(ga, "parent_id", &sf.parent_id, 0,
                                      usage_sf_create,
                                      "skill_folder create") < 0) {
                ret = EXIT_INVALID;
                goto cleanup_sf_create;
            }
        }

        VLOG(1, "skill_folder create: name=%s parent_id=%d",
             sf.name ? sf.name : "(missing)",
             sf.parent_id);

        VLOG(2, "  params: name=%s parent_id=%d fields=%s "
                "no_nulls=%d id_only=%d table=%d",
             sf.name ? sf.name : "(null)", sf.parent_id,
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p json_owned=%d sf=%p name=%p",
             (const void *)ga, json_owned, (const void *)&sf,
             (const void *)sf.name);

        /* ── required-field validation ────────────────────────────── */
        if (!sf.name || !*sf.name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            emit_error("missing required field: name");
            usage_sf_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_sf_create;
        }

        /* ── optional-field validation ────────────────────────────── */
        if (sf.parent_id < 0) {
            sf.parent_id = 0;
        }

        int out_id = 0;
        int rc = acta_db_skill_folder_create(db, sf.name, sf.parent_id, &out_id);

        VLOG(3, "  acta_db_skill_folder_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = finish_op_error(db, rc, "skill_folder create");
            goto cleanup_sf_create;
        }

        VLOG(1, "  created skill_folder id=%d", out_id);

        emit_ok_id(gopts, out_id);

        ret = EXIT_OK;
        goto cleanup_sf_create;

    cleanup_sf_create:
        if (json_owned) {
            free(sf.name);
        }
        return ret;
    }


    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_sf_get, "skill_folder get", &id))
            return EXIT_INVALID;

        VLOG(1, "skill_folder get: fetching id=%d", id);

        int err = 0;
        skill_folder_t *c = acta_db_skill_folder_get(db, id, &err);

        VLOG(3, "  acta_db_skill_folder_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        int rc = load_row_or_notfound(db, err, c, id, sf_free_wrap,
                                      "skill_folder get", "skill_folder");
        if (rc)
            return rc;

        vlog_sf_fields("  result", c);
        vlog_sf_raw("  raw", c, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", c->id);
        } else if (gopts->table) {
            sf_table(stdout, NULL, 1);
            sf_table(stdout, c, 0);
        } else {
            sf_to_json(stdout, c, gopts);
            fputc('\n', stdout);
        }
        acta_db_skill_folder_free(c);
        return EXIT_OK;
    }

    /* ── list [parent_id] ─────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *parent_str = cmd_args_next_positional(ga);
        int parent_id = 0;
        int has_parent = 0;

        if (parent_str) {
            if (strcmp(parent_str, "all") == 0) {
                has_parent = 0;
            } else {
                if (!parse_folder_id(parent_str, &parent_id)) {
                    VLOG(1, "  ERROR: <parent_id> must be a non-negative integer, got '%s'", parent_str);
                    emit_error("<parent_id> must be a non-negative integer");
                    usage_sf_list(stderr);
                    return EXIT_INVALID;
                }
                has_parent = 1;
            }
        }

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_sf_list, "skill_folder list") < 0)
            return EXIT_INVALID;

        if (has_parent)
            VLOG(1, "skill_folder list: parent_id=%d offset=%d limit=%d",
                 parent_id, offset, limit);
        else
            VLOG(1, "skill_folder list: all offset=%d limit=%d", offset, limit);

        VLOG(2, "  full: parent_id=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             has_parent ? parent_id : -1,
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  has_parent=%d parent_id=%d offset=%d limit=%d",
             has_parent, parent_id, offset, limit);

        if (gopts->count) {
            int err = 0;
            int n;
            if (has_parent)
                n = acta_db_skill_folder_count_children(db, parent_id, &err);
            else
                n = acta_db_skill_folder_count_all(db, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "skill_folder count");
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        skill_folder_t **items;
        if (has_parent) {
            items = acta_db_skill_folder_list_children(db, parent_id,
                                                       offset, limit,
                                                       &out_count, &err);
        } else {
            items = acta_db_skill_folder_list_all(db,
                                                  offset, limit,
                                                  &out_count, &err);
        }

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_skill_folder_list_free(items, out_count);
            return finish_op_error(db, err, "skill_folder list");
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_sf_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            sf_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                sf_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                sf_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_skill_folder_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count [parent_id] ────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *parent_str = cmd_args_next_positional(ga);
        int parent_id = 0;
        int has_parent = 0;

        if (parent_str) {
            if (strcmp(parent_str, "all") == 0) {
                has_parent = 0;
            } else {
                if (!parse_folder_id(parent_str, &parent_id)) {
                    VLOG(1, "  ERROR: <parent_id> must be a non-negative integer, got '%s'", parent_str);
                    emit_error("<parent_id> must be a non-negative integer");
                    usage_sf_count(stderr);
                    return EXIT_INVALID;
                }
                has_parent = 1;
            }
        }

        if (has_parent)
            VLOG(1, "skill_folder count: parent_id=%d", parent_id);
        else
            VLOG(1, "skill_folder count: all");

        VLOG(2, "  has_parent=%d parent_id=%d", has_parent, parent_id);

        int err = 0;
        int n;
        if (has_parent)
            n = acta_db_skill_folder_count_children(db, parent_id, &err);
        else
            n = acta_db_skill_folder_count_all(db, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "skill_folder count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── rename <id> --name <new> ─────────────────────────────────── */
    if (strcmp(action, "rename") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_sf_rename, "skill_folder rename", &id))
            return EXIT_INVALID;

        const char *f_name = NULL;
        if (require_flag(ga, "name", &f_name, usage_sf_rename,
                         "skill_folder rename") < 0)
            return EXIT_INVALID;
        if (!f_name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            emit_error("missing required flag: --name");
            usage_sf_rename(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "skill_folder rename: id=%d name=%s", id, f_name);
        VLOG(2, "  params: id=%d name=%s", id, f_name);
        VLOG(3, "  ga=%p f_name=%p", (const void *)ga, (const void *)f_name);

        int rc = acta_db_skill_folder_rename(db, id, f_name);
        VLOG(3, "  acta_db_skill_folder_rename → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill_folder rename");
        }

        VLOG(1, "  renamed skill_folder id=%d → %s", id, f_name);
        emit_ok_id(gopts, id);
        return EXIT_OK;
    }

    /* ── move <id> --parent_id <pid> ──────────────────────────────── */
    if (strcmp(action, "move") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_sf_move, "skill_folder move", &id))
            return EXIT_INVALID;

        int new_parent_id = 0;   /* 0 == root */
        if (parse_nonneg_int_flag(ga, "parent_id", &new_parent_id, 0,
                                  usage_sf_move, "skill_folder move") < 0)
            return EXIT_INVALID;

        VLOG(1, "skill_folder move: id=%d → parent_id=%d", id, new_parent_id);
        VLOG(2, "  params: id=%d new_parent_id=%d", id, new_parent_id);
        VLOG(3, "  id=%d new_parent_id=%d db=%p",
             id, new_parent_id, (const void *)db);

        int rc = acta_db_skill_folder_move_to(db, id, new_parent_id);
        VLOG(3, "  acta_db_skill_folder_move_to → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill_folder move");
        }

        VLOG(1, "  moved skill_folder id=%d → parent_id=%d", id, new_parent_id);
        emit_ok_folder(gopts, id, new_parent_id);
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_sf_delete, "skill_folder delete", &id))
            return EXIT_INVALID;

        VLOG(1, "skill_folder delete: id=%d", id);
        VLOG(2, "  params: id=%d", id);

        int rc = acta_db_skill_folder_soft_delete(db, id);
        VLOG(3, "  acta_db_skill_folder_soft_delete → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill_folder delete");
        }

        VLOG(1, "  deleted skill_folder id=%d", id);
        emit_deleted();
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_sf_restore, "skill_folder restore", &id))
            return EXIT_INVALID;

        VLOG(1, "skill_folder restore: id=%d", id);
        VLOG(2, "  params: id=%d", id);

        int rc = acta_db_skill_folder_restore(db, id);
        VLOG(3, "  acta_db_skill_folder_restore → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill_folder restore");
        }

        VLOG(1, "  restored skill_folder id=%d", id);
        emit_ok_id(gopts, id);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("skill_folder", action, "acta_cli skill_folder help",
                          skill_folder_actions, SF_ACTIONS);
}
