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
 *  Usage: set vlog_gopts at the top of cmd_model, then call
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
        fprintf(f, " %4s  %8s  %-20s  %-20s  %-10s  %-20s  %-30s  %-19s\n",
                "ID", "FOLDER", "NAME", "DESCRIPTION", "BACKEND",
                "BASE_URL", "MODEL_IDENTIFIER", "CREATED_AT");
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
};
#define MODEL_ACTIONS (sizeof(model_actions) / sizeof(model_actions[0]))

int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        const char *f_name        = cmd_args_flag(ga, "name", 1);
        const char *f_folder_id   = cmd_args_flag(ga, "folder-id", 1);
        const char *f_description = cmd_args_flag(ga, "description", 1);
        const char *f_backend     = cmd_args_flag(ga, "backend", 1);
        const char *f_base_url    = cmd_args_flag(ga, "base-url", 1);
        const char *f_model_ident = cmd_args_flag(ga, "model-identifier", 1);
        const char *f_config      = cmd_args_flag(ga, "configuration", 1);

        VLOG(1, "model create: name=%s backend=%s model_identifier=%s",
             f_name        ? f_name        : "(missing)",
             f_backend     ? f_backend     : "(missing)",
             f_model_ident ? f_model_ident : "(missing)");

        VLOG(2, "  params: name=%s folder_id=%s description=%s backend=%s "
                "base_url=%s model_identifier=%s configuration=%s "
                "fields=%s no_nulls=%d id_only=%d table=%d",
             f_name        ? f_name        : "(null)",
             f_folder_id   ? f_folder_id   : "(null)",
             f_description ? f_description : "(null)",
             f_backend     ? f_backend     : "(null)",
             f_base_url    ? f_base_url    : "(null)",
             f_model_ident ? f_model_ident : "(null)",
             f_config      ? f_config      : "(null)",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p name=%p folder_id=%p backend=%p "
                "model_ident=%p config=%p",
             (const void *)ga, (const void *)f_name,
             (const void *)f_folder_id, (const void *)f_backend,
             (const void *)f_model_ident, (const void *)f_config);

        if (gopts->json_input) {
            VLOG(1, "  using --json input (not yet implemented)");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"--json input not yet implemented for model\"}\n");
            return EXIT_INVALID;
        }

        if (!f_name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
            return EXIT_INVALID;
        }
        if (!f_backend) {
            VLOG(1, "  ERROR: missing required field 'backend'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: backend\"}\n");
            return EXIT_INVALID;
        }
        if (!f_model_ident) {
            VLOG(1, "  ERROR: missing required field 'model-identifier'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: model-identifier\"}\n");
            return EXIT_INVALID;
        }

        model_t m = {0};
        m.name             = (char *)f_name;
        m.folder_id        = f_folder_id ? atoi(f_folder_id) : 0;
        m.description      = (char *)f_description;
        m.backend          = (char *)f_backend;
        m.base_url         = (char *)f_base_url;
        m.model_identifier = (char *)f_model_ident;
        m.configuration    = (char *)f_config;

        vlog_model_fields("  pre-create", &m);
        VLOG(3, "  m=%p &out_id=%p",
             (const void *)&m, (const void *)&m.id);

        int out_id = 0;
        int rc = acta_db_model_create(db, &m, &out_id);

        VLOG(3, "  acta_db_model_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  created model id=%d", out_id);

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
            VLOG(1, "model get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model get: invalid id=%s", id_str);
            return EXIT_INVALID;
        }
        const char *s_live = cmd_args_flag(ga, "live", 0);

        VLOG(1, "model get: fetching id=%d live=%d", id, s_live != NULL);

        int err = 0;
        model_t *m = s_live
            ? acta_db_model_get_live(db, id, &err)
            : acta_db_model_get(db, id, &err);

        VLOG(3, "  acta_db_model_get(%d, live=%d) → ptr=%p err=%d",
             id, s_live != NULL, (const void *)m, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_model_free(m);
            return map_rc_to_exit(err);
        }
        if (!m) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

        vlog_model_fields("  result", m);
        vlog_model_raw("  raw", m, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", m->id);
        } else if (gopts->table) {
            model_table(stdout, NULL, 1);
            model_table(stdout, m, 0);
        } else {
            model_to_json(stdout, m, gopts);
            fputc('\n', stdout);
        }
        acta_db_model_free(m);
        return EXIT_OK;
    }

    /* ── update <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "update") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model update: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model update: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        const char *f_name        = cmd_args_flag(ga, "name", 1);
        const char *f_folder_id   = cmd_args_flag(ga, "folder-id", 1);
        const char *f_description = cmd_args_flag(ga, "description", 1);
        const char *f_backend     = cmd_args_flag(ga, "backend", 1);
        const char *f_base_url    = cmd_args_flag(ga, "base-url", 1);
        const char *f_model_ident = cmd_args_flag(ga, "model-identifier", 1);
        const char *f_config      = cmd_args_flag(ga, "configuration", 1);

        /* At least one field must be provided for update. */
        if (!f_name && !f_description && !f_backend && !f_base_url &&
            !f_model_ident && !f_config && !f_folder_id) {
            VLOG(1, "  ERROR: no fields provided for update");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"at least one field required for update\"}\n");
            return EXIT_INVALID;
        }

        VLOG(1, "model update: id=%d name=%s backend=%s model_identifier=%s",
             id,
             f_name        ? f_name        : "(unchanged)",
             f_backend     ? f_backend     : "(unchanged)",
             f_model_ident ? f_model_ident : "(unchanged)");

        VLOG(2, "  params: name=%s folder_id=%s description=%s backend=%s "
                "base_url=%s model_identifier=%s configuration=%s",
             f_name        ? f_name        : "(null)",
             f_folder_id   ? f_folder_id   : "(null)",
             f_description ? f_description : "(null)",
             f_backend     ? f_backend     : "(null)",
             f_base_url    ? f_base_url    : "(null)",
             f_model_ident ? f_model_ident : "(null)",
             f_config      ? f_config      : "(null)");

        VLOG(3, "  id=%d ga=%p", id, (const void *)ga);

        /*
         * For a full update we need the current row to fill in any
         * fields the caller did not supply.  Fetch first.
         */
        int err = 0;
        model_t *cur = acta_db_model_get_live(db, id, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED fetching current row err=%d", err);
            acta_db_model_free(cur);
            return map_rc_to_exit(err);
        }
        if (!cur) {
            VLOG(1, "  not found or already deleted (id=%d)", id);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_NOT_FOUND\",\"code\":-5,"
                "\"message\":\"model not found\"}\n");
            return EXIT_NOT_FOUND;
        }

        model_t m = *cur;   /* shallow copy; we will override fields below */

        if (f_name)
            m.name = (char *)f_name;
        if (f_folder_id)
            m.folder_id = atoi(f_folder_id);
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

        /* m shares strings with cur; free cur once. */
        acta_db_model_free(cur);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  updated model id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model delete: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model delete: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "model delete: id=%d", id);
        VLOG(3, "  id=%d db=%p", id, (const void *)db);

        int rc = acta_db_model_soft_delete(db, id);

        VLOG(3, "  acta_db_model_soft_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  deleted model id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model restore: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "model restore: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "model restore: id=%d", id);
        VLOG(3, "  id=%d db=%p", id, (const void *)db);

        int rc = acta_db_model_restore(db, id);

        VLOG(3, "  acta_db_model_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  restored model id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── move <id> --folder-id <fid> ──────────────────────────────── */
    if (strcmp(action, "move") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "model move: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int model_id = atoi(id_str);
        if (model_id <= 0) {
            VLOG(1, "model move: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        const char *f_folder_id = cmd_args_flag(ga, "folder-id", 1);
        int folder_id = f_folder_id ? atoi(f_folder_id) : 0;

        VLOG(1, "model move: id=%d → folder_id=%d", model_id, folder_id);
        VLOG(2, "  folder-id raw=%s", f_folder_id ? f_folder_id : "(root)");
        VLOG(3, "  model_id=%d folder_id=%d db=%p",
             model_id, folder_id, (const void *)db);

        int rc = acta_db_model_move_to_folder(db, model_id, folder_id);

        VLOG(3, "  acta_db_model_move_to_folder(%d, %d) → rc=%d",
             model_id, folder_id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  moved model id=%d → folder_id=%d", model_id, folder_id);
        fprintf(stdout, "{\"id\":%d,\"folder_id\":%d}\n", model_id, folder_id);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *f_folder  = cmd_args_flag(ga, "folder-id", 1);
        const char *s_off     = cmd_args_flag(ga, "offset", 1);
        const char *s_lim     = cmd_args_flag(ga, "limit", 1);
        const char *s_count   = cmd_args_flag(ga, "count", 0);

        int folder_id = f_folder ? atoi(f_folder) : -1;  /* -1 = all */
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

        VLOG(1, "model list: folder_id=%d offset=%d limit=%d",
             folder_id, offset, limit);

        VLOG(2, "  full: folder_id=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             folder_id, offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  folder_id=%d offset=%d limit=%d db=%p",
             folder_id, offset, limit, (const void *)db);

        /* ── --count short-circuit ── */
        if (gopts->count || s_count) {
            int err = 0;
            int n;
            if (folder_id >= 0)
                n = acta_db_model_count_in_folder(db, folder_id, &err);
            else
                n = acta_db_model_count_all(db, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        /* ── normal list ── */
        int out_count = 0, err = 0;
        model_t **items;

        if (folder_id >= 0)
            items = acta_db_model_list_in_folder(db, folder_id,
                                                 offset, limit,
                                                 &out_count, &err);
        else
            items = acta_db_model_list_all(db,
                                           offset, limit,
                                           &out_count, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_model_list_free(items, out_count);
            return map_rc_to_exit(err);
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
        const char *f_folder = cmd_args_flag(ga, "folder-id", 1);
        int folder_id = f_folder ? atoi(f_folder) : -1;  /* -1 = all */

        VLOG(1, "model count: folder_id=%d", folder_id);
        VLOG(2, "  folder-id raw=%s", f_folder ? f_folder : "(all)");
        VLOG(3, "  folder_id=%d db=%p", folder_id, (const void *)db);

        int err = 0;
        int n;
        if (folder_id >= 0)
            n = acta_db_model_count_in_folder(db, folder_id, &err);
        else
            n = acta_db_model_count_all(db, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "model: unknown action '%s'", action ? action : "(null)");
    return action_err("model", action, model_actions, MODEL_ACTIONS);
}
