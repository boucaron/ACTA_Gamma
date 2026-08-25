/* skill_cmd.c */
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

static const global_opts_t *vlog_gopts;   /* set once per cmd_* call */

#define VLOG(lvl, fmt, ...)                                              \
    do {                                                                 \
        if (vlog_gopts && vlog_gopts->verbose >= (lvl)) {                 \
            fprintf(stderr, "[v" #lvl "] " fmt "\n", ##__VA_ARGS__);     \
        }                                                                \
    } while (0)

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_skill_fields(const char *tag, const skill_t *s)
{
    VLOG(2, "%s: id=%d folder_id=%d name=%s description=%s prompt_template=%s "
         "output_schema=%s created_at=%s updated_at=%s deleted_at=%s",
         tag,
         s->id,
         s->folder_id,
         s->name            ? s->name            : "(null)",
         s->description     ? s->description     : "(null)",
         s->prompt_template ? s->prompt_template : "(null)",
         s->output_schema   ? s->output_schema   : "(null)",
         s->created_at      ? s->created_at      : "(null)",
         s->updated_at      ? s->updated_at      : "(null)",
         s->deleted_at      ? s->deleted_at      : "(null)");
}

static void vlog_skill_raw(const char *tag, const skill_t *s, int rc)
{
    VLOG(3, "%s: skill=%p id=%d rc=%d",
         tag, (const void *)s, s ? s->id : -1, rc);
}

/* ── skill_t → JSON object ────────────────────────────────────────── */

static void skill_to_json(FILE *f, const skill_t *s, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", s->id);
    }
    if ((!fl || fields_has(fl, "folder_id")) && !(gopts->no_nulls && s->folder_id == 0)) {
        if (shown++) fputs(", ", f);
        fputs("\"folder_id\":", f);
        if (s->folder_id == 0) fputs("null", f);
        else                  fprintf(f, "%d", s->folder_id);
    }
    if ((!fl || fields_has(fl, "name")) && !(gopts->no_nulls && !s->name)) {
        if (shown++) fputs(", ", f);
        fputs("\"name\":", f);
        if (s->name) json_str(f, s->name); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "description")) && !(gopts->no_nulls && !s->description)) {
        if (shown++) fputs(", ", f);
        fputs("\"description\":", f);
        if (s->description) json_str(f, s->description); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "prompt_template")) && !(gopts->no_nulls && !s->prompt_template)) {
        if (shown++) fputs(", ", f);
        fputs("\"prompt_template\":", f);
        if (s->prompt_template) json_str(f, s->prompt_template); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "output_schema")) && !(gopts->no_nulls && !s->output_schema)) {
        if (shown++) fputs(", ", f);
        fputs("\"output_schema\":", f);
        if (s->output_schema) json_str(f, s->output_schema); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !s->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (s->created_at) json_str(f, s->created_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "updated_at")) && !(gopts->no_nulls && !s->updated_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"updated_at\":", f);
        if (s->updated_at) json_str(f, s->updated_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "deleted_at")) && !(gopts->no_nulls && !s->deleted_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"deleted_at\":", f);
        if (s->deleted_at) json_str(f, s->deleted_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void skill_table(FILE *f, const skill_t *s, int header)
{
    if (header) {
        fprintf(f, " %4s  %10s  %-20s  %-38s  %-20s  %-19s  %-19s\n",
                "ID", "FOLDER_ID", "NAME", "DESCRIPTION", "PROMPT_TEMPLATE",
                "CREATED_AT", "UPDATED_AT");
        return;
    }
    char idb[16];
    char fdb[16];
    snprintf(idb, sizeof idb, "%d", s->id);
    snprintf(fdb, sizeof fdb, s->folder_id ? "%d" : "-", s->folder_id);
    fprintf(f, " %4s  ", idb);
    fprintf(f, " %10s  ", fdb);
    tcol(f, s->name,            20);
    tcol(f, s->description,     38);
    tcol(f, s->prompt_template, 20);
    tcol(f, s->created_at,      19);
    tcol(f, s->updated_at,      19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t skill_actions[] = {
    { "create",  "create a new skill"             },
    { "get",     "fetch a skill by id"            },
    { "update",  "update an existing skill"       },
    { "delete",  "remove a skill"                 },
    { "restore", "restore a deleted skill"        },
    { "move",    "move a skill to another folder" },
    { "list",    "list skills"                    },
    { "count",   "count skills"                   },
};
#define SKILL_ACTIONS (sizeof(skill_actions) / sizeof(skill_actions[0]))

int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        skill_t s = {0};
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
            VLOG(1, "skill create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_skill(blob, &s) != 0) {
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
            const char *f_name    = cmd_args_flag(ga, "name", 1);
            const char *f_prompt  = cmd_args_flag(ga, "prompt_template", 1);
            const char *f_folder  = cmd_args_flag(ga, "folder_id", 1);
            const char *f_desc    = cmd_args_flag(ga, "description", 1);
            const char *f_schema  = cmd_args_flag(ga, "output_schema", 1);

            s.name            = (char *)f_name;
            s.prompt_template = (char *)f_prompt;
            s.description     = (char *)f_desc;
            s.output_schema   = (char *)f_schema;
            s.folder_id       = f_folder ? atoi(f_folder) : 0;
        }

        VLOG(1, "skill create: name=%s prompt_template=%s folder_id=%d",
             s.name ? s.name : "(missing)",
             s.prompt_template ? s.prompt_template : "(missing)",
             s.folder_id);

        VLOG(2, "  params: name=%s folder_id=%d description=%s "
                "prompt_template=%s output_schema=%s "
                "fields=%s no_nulls=%d id_only=%d table=%d",
             s.name ? s.name : "(null)",
             s.folder_id,
             s.description ? s.description : "(null)",
             s.prompt_template ? s.prompt_template : "(null)",
             s.output_schema ? s.output_schema : "(null)",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p json_owned=%d s=%p name=%p prompt=%p "
                "folder=%d desc=%p schema=%p",
             (const void *)ga, json_owned, (const void *)&s,
             (const void *)s.name,
             (const void *)s.prompt_template,
             s.folder_id,
             (const void *)s.description,
             (const void *)s.output_schema);

        /* ── required-field validation (handler, not parser) ─────── */
        if (!s.name || !*s.name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
            ret = EXIT_INVALID;
            goto cleanup_skill_create;
        }
        if (!s.prompt_template || !*s.prompt_template) {
            VLOG(1, "  ERROR: missing required field 'prompt_template'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: prompt_template\"}\n");
            ret = EXIT_INVALID;
            goto cleanup_skill_create;
        }

        /* ── optional-field validation ───────────────────────────── */
        if (s.folder_id < 0) s.folder_id = 0;

        vlog_skill_fields("  pre-create", &s);

        int out_id = 0;
        int rc = acta_db_skill_create(db, &s, &out_id);

        VLOG(3, "  acta_db_skill_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = map_rc_to_exit(rc);
            goto cleanup_skill_create;
        }

        VLOG(1, "  created skill id=%d", out_id);

        if (gopts->id_only)
            fprintf(stdout, "%d\n", out_id);
        else
            fprintf(stdout, "{\"id\":%d}\n", out_id);

        ret = EXIT_OK;
        goto cleanup_skill_create;

    cleanup_skill_create:
        if (json_owned) {
            free(s.name);
            free(s.description);
            free(s.prompt_template);
            free(s.output_schema);
            free(s.created_at);
            free(s.updated_at);
            free(s.deleted_at);
        }
        return ret;
    }



    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill get: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        const char *f_inc_del = cmd_args_flag(ga, "include-deleted", 0);

        VLOG(1, "skill get: fetching id=%d include_deleted=%d",
             id, f_inc_del ? 1 : 0);

        int err = 0;
        skill_t *s = f_inc_del
            ? acta_db_skill_get(db, id, &err)
            : acta_db_skill_get_live(db, id, &err);

        VLOG(3, "  acta_db_skill_get(_live)(%d) → ptr=%p err=%d",
             id, (const void *)s, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_skill_free(s);
            return map_rc_to_exit(err);
        }
        if (!s) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

        vlog_skill_fields("  result", s);
        vlog_skill_raw("  raw", s, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", s->id);
        } else if (gopts->table) {
            skill_table(stdout, NULL, 1);
            skill_table(stdout, s, 0);
        } else {
            skill_to_json(stdout, s, gopts);
            fputc('\n', stdout);
        }
        acta_db_skill_free(s);
        return EXIT_OK;
    }

    /* ── update <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "update") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill update: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill update: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        const char *f_name    = cmd_args_flag(ga, "name", 1);
        const char *f_prompt  = cmd_args_flag(ga, "prompt_template", 1);
        const char *f_folder  = cmd_args_flag(ga, "folder_id", 1);
        const char *f_desc    = cmd_args_flag(ga, "description", 1);
        const char *f_schema  = cmd_args_flag(ga, "output-schema", 1);

        VLOG(1, "skill update: id=%d name=%s prompt_template=%s",
             id,
             f_name   ? f_name   : "(missing)",
             f_prompt ? f_prompt : "(missing)");

        VLOG(2, "  params: id=%d name=%s folder_id=%s description=%s "
                "prompt_template=%s output_schema=%s",
             id,
             f_name   ? f_name   : "(null)",
             f_folder ? f_folder : "(null)",
             f_desc   ? f_desc   : "(null)",
             f_prompt ? f_prompt : "(null)",
             f_schema ? f_schema : "(null)");

        VLOG(3, "  raw: ga=%p id=%d name=%p prompt=%p folder=%p desc=%p schema=%p",
             (const void *)ga, id, (const void *)f_name,
             (const void *)f_prompt, (const void *)f_folder,
             (const void *)f_desc, (const void *)f_schema);

        if (gopts->json_input) {
            VLOG(1, "  using --json input (not yet implemented)");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"--json input not yet implemented for skill update\"}\n");
            return EXIT_INVALID;
        }

        if (!f_name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
            return EXIT_INVALID;
        }
        if (!f_prompt) {
            VLOG(1, "  ERROR: missing required field 'prompt_template'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: prompt_template\"}\n");
            return EXIT_INVALID;
        }

        skill_t s = {0};
        s.id = id;
        if (f_folder) s.folder_id = atoi(f_folder);
        s.name            = (char *)f_name;
        s.description     = (char *)f_desc;
        s.prompt_template = (char *)f_prompt;
        s.output_schema   = (char *)f_schema;

        vlog_skill_fields("  pre-update", &s);

        int rc = acta_db_skill_update(db, &s);

        VLOG(3, "  acta_db_skill_update → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  updated skill id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill delete: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill delete: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "skill delete: id=%d", id);

        int rc = acta_db_skill_soft_delete(db, id);

        VLOG(3, "  acta_db_skill_soft_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  deleted skill id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill restore: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill restore: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "skill restore: id=%d", id);

        int rc = acta_db_skill_restore(db, id);

        VLOG(3, "  acta_db_skill_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  restored skill id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);
        return EXIT_OK;
    }

    /* ── move <skill_id> --folder_id <folder_id> ──────────────────── */
    if (strcmp(action, "move") == 0) {
        const char *skill_id_str = cmd_args_next_positional(ga);
        if (!skill_id_str) {
            VLOG(1, "skill move: ERROR missing <skill_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <skill_id>\"}\n");
            return EXIT_INVALID;
        }
        int skill_id = atoi(skill_id_str);
        if (skill_id <= 0) {
            VLOG(1, "skill move: invalid skill_id=%s", skill_id_str);
            return EXIT_INVALID;
        }

        const char *f_folder = cmd_args_flag(ga, "folder_id", 1);
        int folder_id = 0;  /* 0 = root (no folder) */
        if (f_folder) folder_id = atoi(f_folder);

        VLOG(1, "skill move: skill_id=%d folder_id=%d", skill_id, folder_id);

        VLOG(2, "  params: skill_id=%d folder_id=%d", skill_id, folder_id);

        VLOG(3, "  raw: ga=%p skill_id=%d folder_id=%d",
             (const void *)ga, skill_id, folder_id);

        int rc = acta_db_skill_move_to_folder(db, skill_id, folder_id);

        VLOG(3, "  acta_db_skill_move_to_folder(%d, %d) → rc=%d",
             skill_id, folder_id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  moved skill id=%d → folder_id=%d", skill_id, folder_id);
        fprintf(stdout, "{\"id\":%d,\"folder_id\":%d}\n",
                skill_id, folder_id ? folder_id : 0);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *f_folder  = cmd_args_flag(ga, "folder_id", 0);
        const char *s_off     = cmd_args_flag(ga, "offset", 1);
        const char *s_lim     = cmd_args_flag(ga, "limit", 1);
        const char *s_count   = cmd_args_flag(ga, "count", 0);
        const char *f_all     = cmd_args_flag(ga, "all", 0);

        int offset = 0, limit = 0;
        int folder_id = 0;
        int in_folder = 0;

        if (f_folder) {
            folder_id = atoi(f_folder);
            in_folder = 1;
        }

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

        VLOG(1, "skill list: folder_id=%s all=%d offset=%d limit=%d",
             f_folder ? f_folder : (f_all ? "(any/all)" : "(root)"),
             f_all ? 1 : 0,
             offset, limit);

        VLOG(2, "  full: folder_id=%s all=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             f_folder ? f_folder : "(null)",
             f_all ? 1 : 0,
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  folder_id=%d all=%d offset=%d limit=%d",
             folder_id, f_all ? 1 : 0, offset, limit);

        if (gopts->count || s_count) {
            int err = 0;
            int n = (in_folder && !f_all)
                ? acta_db_skill_count_in_folder(db, folder_id, &err)
                : acta_db_skill_count_all(db, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        skill_t **items;

        if (in_folder && !f_all) {
            items = acta_db_skill_list_in_folder(db, folder_id,
                                                 offset, limit,
                                                 &out_count, &err);
        } else {
            items = acta_db_skill_list_all(db, offset, limit,
                                           &out_count, &err);
        }

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_skill_list_free(items, out_count);
            return map_rc_to_exit(err);
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_skill_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            skill_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                skill_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                skill_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_skill_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count ────────────────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *f_folder = cmd_args_flag(ga, "folder_id", 0);
        const char *f_all    = cmd_args_flag(ga, "all", 0);

        int folder_id = 0;
        int in_folder = 0;
        if (f_folder) {
            folder_id = atoi(f_folder);
            in_folder = 1;
        }

        VLOG(1, "skill count: folder_id=%s all=%d",
             f_folder ? f_folder : (f_all ? "(any/all)" : "(root)"),
             f_all ? 1 : 0);

        VLOG(2, "  folder_id=%d all=%d", folder_id, f_all ? 1 : 0);

        int err = 0;
        int n = (in_folder && !f_all)
            ? acta_db_skill_count_in_folder(db, folder_id, &err)
            : acta_db_skill_count_all(db, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "skill: unknown action '%s'", action ? action : "(null)");
    return action_err("skill", action, skill_actions, SKILL_ACTIONS);
}
