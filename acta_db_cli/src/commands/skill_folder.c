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
 *  Usage: set vlog_gopts at the top of cmd_skill_folder, then call
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
    { "create",  "create a new folder"         },
    { "get",     "fetch a folder by id"        },
    { "list",    "list folders (all or by parent)" },
    { "count",   "count folders"               },
    { "rename",  "rename a folder"             },
    { "move",    "move a folder to a new parent" },
    { "delete",  "soft-delete a folder"        },
    { "restore", "restore a soft-deleted folder" }
};
#define SF_ACTIONS (sizeof(skill_folder_actions) / sizeof(skill_folder_actions[0]))

int cmd_skill_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                     db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        const char *f_name      = cmd_args_flag(ga, "name", 1);
        const char *f_parent   = cmd_args_flag(ga, "parent-id", 1);

        int parent_id = 0;
        if (f_parent) {
            parent_id = atoi(f_parent);
            if (parent_id < 0) parent_id = 0;
        }

        VLOG(1, "folder create: name=%s parent_id=%d",
             f_name ? f_name : "(missing)", parent_id);

        VLOG(2, "  params: name=%s parent_id=%d fields=%s "
                "no_nulls=%d id_only=%d table=%d",
             f_name ? f_name : "(null)", parent_id,
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p f_name=%p f_parent=%p",
             (const void *)ga, (const void *)f_name, (const void *)f_parent);

        if (gopts->json_input) {
            VLOG(1, "  using --json input (not yet implemented)");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"--json input not yet implemented for skill_folder\"}\n");
            return EXIT_INVALID;
        }

        if (!f_name || !*f_name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
            return EXIT_INVALID;
        }

        int out_id = 0;
        int rc = acta_db_skill_folder_create(db, f_name, parent_id, &out_id);

        VLOG(3, "  acta_db_skill_folder_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  created folder id=%d", out_id);

        if (gopts->id_only)
            fprintf(stdout, "%d\n", out_id);
        else
            fprintf(stdout, "{\"id\":%d}\n", out_id);
        return EXIT_OK;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "folder get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "folder get: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "folder get: fetching id=%d", id);

        int err = 0;
        skill_folder_t *c = acta_db_skill_folder_get(db, id, &err);

        VLOG(3, "  acta_db_skill_folder_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_skill_folder_free(c);
            return map_rc_to_exit(err);
        }
        if (!c) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

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
                parent_id = atoi(parent_str);
                if (parent_id < 0) parent_id = 0;
                has_parent = 1;
            }
        }

        const char *s_off = cmd_args_flag(ga, "offset", 1);
        const char *s_lim = cmd_args_flag(ga, "limit", 1);
        const char *s_count = cmd_args_flag(ga, "count", 0);

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
            limit = (int)v;
        }

        if (has_parent)
            VLOG(1, "folder list: parent_id=%d offset=%d limit=%d",
                 parent_id, offset, limit);
        else
            VLOG(1, "folder list: all offset=%d limit=%d", offset, limit);

        VLOG(2, "  full: parent_id=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             has_parent ? parent_id : -1,
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  has_parent=%d parent_id=%d offset=%d limit=%d",
             has_parent, parent_id, offset, limit);

        if (gopts->count || s_count) {
            int err = 0;
            int n;
            if (has_parent)
                n = acta_db_skill_folder_count_children(db, parent_id, &err);
            else
                n = acta_db_skill_folder_count_all(db, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
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
            return map_rc_to_exit(err);
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
                parent_id = atoi(parent_str);
                if (parent_id < 0) parent_id = 0;
                has_parent = 1;
            }
        }

        if (has_parent)
            VLOG(1, "folder count: parent_id=%d", parent_id);
        else
            VLOG(1, "folder count: all");

        VLOG(2, "  has_parent=%d parent_id=%d", has_parent, parent_id);

        int err = 0;
        int n;
        if (has_parent)
            n = acta_db_skill_folder_count_children(db, parent_id, &err);
        else
            n = acta_db_skill_folder_count_all(db, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── rename <id> --name <new> ─────────────────────────────────── */
    if (strcmp(action, "rename") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "folder rename: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "folder rename: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        const char *f_name = cmd_args_flag(ga, "name", 1);
        if (!f_name || !*f_name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required flag: --name\"}\n");
            return EXIT_INVALID;
        }

        VLOG(1, "folder rename: id=%d name=%s", id, f_name);
        VLOG(2, "  params: id=%d name=%s", id, f_name);
        VLOG(3, "  ga=%p f_name=%p", (const void *)ga, (const void *)f_name);

        int rc = acta_db_skill_folder_rename(db, id, f_name);
        VLOG(3, "  acta_db_skill_folder_rename → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  renamed folder id=%d → %s", id, f_name);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── move <id> --parent-id <pid> ──────────────────────────────── */
    if (strcmp(action, "move") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "folder move: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "folder move: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        const char *f_parent = cmd_args_flag(ga, "parent-id", 1);
        int new_parent_id = 0;
        if (f_parent) {
            new_parent_id = atoi(f_parent);
            if (new_parent_id < 0) new_parent_id = 0;
        }

        VLOG(1, "folder move: id=%d → parent_id=%d", id, new_parent_id);
        VLOG(2, "  params: id=%d new_parent_id=%d", id, new_parent_id);
        VLOG(3, "  ga=%p f_parent=%p", (const void *)ga, (const void *)f_parent);

        int rc = acta_db_skill_folder_move_to(db, id, new_parent_id);
        VLOG(3, "  acta_db_skill_folder_move_to → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  moved folder id=%d → parent_id=%d", id, new_parent_id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "folder delete: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "folder delete: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "folder delete: id=%d", id);
        VLOG(2, "  params: id=%d", id);

        int rc = acta_db_skill_folder_soft_delete(db, id);
        VLOG(3, "  acta_db_skill_folder_soft_delete → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  deleted folder id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "folder restore: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "folder restore: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "folder restore: id=%d", id);
        VLOG(2, "  params: id=%d", id);

        int rc = acta_db_skill_folder_restore(db, id);
        VLOG(3, "  acta_db_skill_folder_restore → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  restored folder id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "skill_folder: unknown action '%s'", action ? action : "(null)");
    return action_err("skill_folder", action, skill_folder_actions, SF_ACTIONS);
}
