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
 *
 *  Usage: set vlog_gopts at the top of cmd_skill, then call
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

/* ══════════════════════════════════════════════════════════════════ */
/*  Usage / help                                                       */
/* ══════════════════════════════════════════════════════════════════ */

/* Non-static: the global dispatch layer can call this for
 *   actagamma_db skill --help                                       */
void skill_usage(FILE *f)
{
    fputs(
"Usage: actagamma_db skill <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new skill\n"
"  get       Fetch a skill by id\n"
"  update    Update an existing skill\n"
"  delete    Soft-delete a skill\n"
"  restore   Restore a deleted skill\n"
"  move      Move a skill to another folder\n"
"  list      List skills\n"
"  count     Count skills\n"
"  help      Show this help\n"
"\n"
"== create ===========================================================\n"
"  Create a new skill.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    actagamma_db skill create \\\n"
"      --name \"Summarize\" \\\n"
"      --prompt_template \"Summarize: {{input}}\" \\\n"
"      --folder_id 3 \\\n"
"      --description \"Summarises long text\" \\\n"
"      --output_schema '{\"type\":\"string\"}'\n"
"        <- flag-based\n"
"\n"
"    cat skill.json | actagamma_db skill create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>             Display name\n"
"    --prompt_template <str>  Prompt template body\n"
"\n"
"  Optional fields:\n"
"    --folder_id <int>        Owning folder (0 = root)\n"
"    --description <str>      Human-readable description\n"
"    --output_schema <json>   Expected output JSON schema\n"
"\n"
"  Options:\n"
"    --json               Read the skill as JSON from stdin\n"
"    --id-only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single skill by its primary key.\n"
"\n"
"    actagamma_db skill get 42\n"
"    actagamma_db skill get 42 --include-deleted\n"
"\n"
"  Options:\n"
"    --include-deleted    Return the row even if soft-deleted\n"
"    --id-only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== update <id> ====================================================\n"
"  Fully replace a skill's fields (all required fields must be\n"
"  re-supplied).\n"
"\n"
"    actagamma_db skill update 42 \\\n"
"      --name \"Summarize v2\" \\\n"
"      --prompt_template \"Summarize (v2): {{input}}\"\n"
"\n"
"  Required fields (same as create):\n"
"    --name <str>             Display name\n"
"    --prompt_template <str>  Prompt template body\n"
"\n"
"  Optional fields:\n"
"    --folder_id <int>        Owning folder (0 = root)\n"
"    --description <str>      Human-readable description\n"
"    --output_schema <json>   Expected output JSON schema\n"
"\n"
"  Options:\n"
"    --json               Read the skill as JSON from stdin\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== delete <id> ====================================================\n"
"  Soft-delete a skill (sets deleted_at; row is retained).\n"
"\n"
"    actagamma_db skill delete 42\n"
"\n"
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted skill.\n"
"\n"
"    actagamma_db skill restore 42\n"
"\n"
"== move <skill_id> ================================================\n"
"  Move a skill into (or out of) a folder.\n"
"\n"
"    actagamma_db skill move 42 --folder_id 3\n"
"    actagamma_db skill move 42 --folder_id 0   # back to root\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Destination folder (required, 0 = root)\n"
"\n"
"== list ===========================================================\n"
"  List skills, optionally filtered by folder.\n"
"\n"
"    actagamma_db skill list\n"
"    actagamma_db skill list --folder_id 3\n"
"    actagamma_db skill list --all --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Only skills in this folder\n"
"    --all                Include skills from all folders\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count ==========================================================\n"
"  Count skills.\n"
"\n"
"    actagamma_db skill count\n"
"    actagamma_db skill count --folder_id 3\n"
"    actagamma_db skill count --all\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Count only skills in this folder\n"
"    --all                Count across all folders\n"
"\n"
"Global options:\n"
"  --table            columnar / plain output instead of JSON\n"
"  --verbose <n>      debug level 0-3 (diagnostics on stderr)\n"
"  --fields <csv>     comma-separated field whitelist\n"
"  --no_nulls         omit null-valued fields from JSON output\n"
"  --id-only          print only the id (create / get)\n"
"\n", f);
}

/* ── per-action usage snippets (printed to stderr on arg errors) ──── */

static void usage_create(FILE *f)
{
    fputs(
"== create ===========================================================\n"
"  Create a new skill.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    actagamma_db skill create \\\n"
"      --name \"Summarize\" \\\n"
"      --prompt_template \"Summarize: {{input}}\" \\\n"
"      --folder_id 3 \\\n"
"      --description \"Summarises long text\" \\\n"
"      --output_schema '{\"type\":\"string\"}'\n"
"        <- flag-based\n"
"\n"
"    cat skill.json | actagamma_db skill create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --name <str>             Display name\n"
"    --prompt_template <str>  Prompt template body\n"
"\n"
"  Optional fields:\n"
"    --folder_id <int>        Owning folder (0 = root)\n"
"    --description <str>      Human-readable description\n"
"    --output_schema <json>   Expected output JSON schema\n"
"\n"
"  Options:\n"
"    --json               Read the skill as JSON from stdin\n"
"    --id-only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single skill by its primary key.\n"
"\n"
"    actagamma_db skill get 42\n"
"    actagamma_db skill get 42 --include-deleted\n"
"\n"
"  Options:\n"
"    --include-deleted    Return the row even if soft-deleted\n"
"    --id-only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_update(FILE *f)
{
    fputs(
"== update <id> ====================================================\n"
"  Fully replace a skill's fields (all required fields must be\n"
"  re-supplied).\n"
"\n"
"    actagamma_db skill update 42 \\\n"
"      --name \"Summarize v2\" \\\n"
"      --prompt_template \"Summarize (v2): {{input}}\"\n"
"\n"
"  Required fields (same as create):\n"
"    --name <str>             Display name\n"
"    --prompt_template <str>  Prompt template body\n"
"\n"
"  Optional fields:\n"
"    --folder_id <int>        Owning folder (0 = root)\n"
"    --description <str>      Human-readable description\n"
"    --output_schema <json>   Expected output JSON schema\n"
"\n"
"  Options:\n"
"    --json               Read the skill as JSON from stdin\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_delete(FILE *f)
{
    fputs(
"== delete <id> ====================================================\n"
"  Soft-delete a skill (sets deleted_at; row is retained).\n"
"\n"
"    actagamma_db skill delete 42\n", f);
}

static void usage_restore(FILE *f)
{
    fputs(
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted skill.\n"
"\n"
"    actagamma_db skill restore 42\n", f);
}

static void usage_move(FILE *f)
{
    fputs(
"== move <skill_id> ================================================\n"
"  Move a skill into (or out of) a folder.\n"
"\n"
"    actagamma_db skill move 42 --folder_id 3\n"
"    actagamma_db skill move 42 --folder_id 0   # back to root\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Destination folder (required, 0 = root)\n", f);
}

static void usage_list(FILE *f)
{
    fputs(
"== list ===========================================================\n"
"  List skills, optionally filtered by folder.\n"
"\n"
"    actagamma_db skill list\n"
"    actagamma_db skill list --folder_id 3\n"
"    actagamma_db skill list --all --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Only skills in this folder\n"
"    --all                Include skills from all folders\n"
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
"== count ==========================================================\n"
"  Count skills.\n"
"\n"
"    actagamma_db skill count\n"
"    actagamma_db skill count --folder_id 3\n"
"    actagamma_db skill count --all\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Count only skills in this folder\n"
"    --all                Count across all folders\n", f);
}

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
    { "delete",  "soft-delete a skill"            },
    { "restore", "restore a deleted skill"        },
    { "move",    "move a skill to another folder" },
    { "list",    "list skills"                    },
    { "count",   "count skills"                   },
    { "help",    "show this help"                 },
};
#define SKILL_ACTIONS (sizeof(skill_actions) / sizeof(skill_actions[0]))

int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── help (subcommand-level; only the bare word "help") ──────── */
    if (strcmp(action, "help") == 0) {
        skill_usage(stdout);
        return EXIT_OK;
    }

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
                usage_create(stderr);
                return EXIT_INVALID;
            }
            VLOG(1, "skill create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_skill(blob, &s) != 0) {
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
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_create;
        }
        if (!s.prompt_template || !*s.prompt_template) {
            VLOG(1, "  ERROR: missing required field 'prompt_template'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: prompt_template\"}\n");
            usage_create(stderr);
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
            usage_get(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill get: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_get(stderr);
            return EXIT_INVALID;
        }

        int f_inc_del = cmd_args_has_flag(ga, "include-deleted");

        VLOG(1, "skill get: fetching id=%d include_deleted=%d",
             id, f_inc_del);

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
        /* ── positional <id> ─────────────────────────────────────── */
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill update: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_update(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill update: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_update(stderr);
            return EXIT_INVALID;
        }

        skill_t s = {0};
        s.id = id;
        int json_owned = 0;
        int ret = EXIT_OK;

        if (gopts->json_input) {
            char *blob = read_stdin_all();
            if (!blob) {
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"failed to read JSON input\"}\n");
                usage_update(stderr);
                return EXIT_INVALID;
            }
            VLOG(1, "skill update: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_skill(blob, &s) != 0) {
                VLOG(1, "  JSON parse error");
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"invalid JSON body\"}\n");
                usage_update(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;

            /* positional <id> is the authority */
            s.id = id;
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

        VLOG(1, "skill update: id=%d name=%s prompt_template=%s folder_id=%d",
             id,
             s.name ? s.name : "(missing)",
             s.prompt_template ? s.prompt_template : "(missing)",
             s.folder_id);

        VLOG(2, "  params: id=%d name=%s folder_id=%d description=%s "
                "prompt_template=%s output_schema=%s "
                "fields=%s no_nulls=%d id_only=%d table=%d",
             id,
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

        /* ── required-field validation (full replacement) ────────── */
        if (!s.name || !*s.name) {
            VLOG(1, "  ERROR: missing required field 'name'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: name\"}\n");
            usage_update(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_update;
        }
        if (!s.prompt_template || !*s.prompt_template) {
            VLOG(1, "  ERROR: missing required field 'prompt_template'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: prompt_template\"}\n");
            usage_update(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_update;
        }

        /* ── optional-field validation ───────────────────────────── */
        if (s.folder_id < 0) s.folder_id = 0;

        vlog_skill_fields("  pre-update", &s);

        int rc = acta_db_skill_update(db, &s);

        VLOG(3, "  acta_db_skill_update → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = map_rc_to_exit(rc);
            goto cleanup_skill_update;
        }

        VLOG(1, "  updated skill id=%d", id);
        fprintf(stdout, "{\"id\":%d}\n", id);

        ret = EXIT_OK;
        goto cleanup_skill_update;

    cleanup_skill_update:
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


    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill delete: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_delete(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill delete: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_delete(stderr);
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
            usage_restore(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill restore: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_restore(stderr);
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
            usage_move(stderr);
            return EXIT_INVALID;
        }
        int skill_id = atoi(skill_id_str);
        if (skill_id <= 0) {
            VLOG(1, "skill move: invalid skill_id=%s", skill_id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <skill_id>: must be a positive integer\"}\n");
            usage_move(stderr);
            return EXIT_INVALID;
        }

        const char *f_folder = cmd_args_flag(ga, "folder_id", 1);
        if (!f_folder) {
            VLOG(1, "skill move: ERROR missing --folder_id");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required flag: --folder_id\"}\n");
            usage_move(stderr);
            return EXIT_INVALID;
        }
        int folder_id = atoi(f_folder);
        if (folder_id < 0) folder_id = 0;  /* 0 = root (no folder) */

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
                skill_id, folder_id);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *f_folder  = cmd_args_flag(ga, "folder_id", 1);
        const char *s_off     = cmd_args_flag(ga, "offset", 1);
        const char *s_lim     = cmd_args_flag(ga, "limit", 1);
        const char *s_count   = cmd_args_flag(ga, "count", 0);
        int has_all = cmd_args_has_flag(ga, "all");

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
                fprintf(stderr,
                    "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                    "\"message\":\"--offset must be a non-negative integer\"}\n");
                usage_list(stderr);
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
                usage_list(stderr);
                return EXIT_INVALID;
            }
            limit = (int)v;
        }

        VLOG(1, "skill list: folder_id=%s all=%d offset=%d limit=%d",
             f_folder ? f_folder : (has_all ? "(any/all)" : "(root)"),
             has_all ? 1 : 0,
             offset, limit);

        VLOG(2, "  full: folder_id=%s all=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             f_folder ? f_folder : "(null)",
             has_all ? 1 : 0,
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  folder_id=%d all=%d offset=%d limit=%d",
             folder_id, has_all ? 1 : 0, offset, limit);

        if (gopts->count || s_count) {
            int err = 0;
            int n = (in_folder && !has_all )
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

        if (in_folder && !has_all) {
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
        const char *f_folder = cmd_args_flag(ga, "folder_id", 1);
        int has_all          = cmd_args_has_flag(ga, "all"); 

        int folder_id = 0;
        int in_folder = 0;
        if (f_folder) {
            folder_id = atoi(f_folder);
            in_folder = 1;
        }

        VLOG(1, "skill count: folder_id=%s all=%d",
             f_folder ? f_folder : (has_all ? "(any/all)" : "(root)"),
             has_all ? 1 : 0);

        VLOG(2, "  folder_id=%d all=%d", folder_id, has_all ? 1 : 0);

        int err = 0;
        int n = (in_folder && !has_all)
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

    /* ── Unknown action: suggest closest match + pointer to help ── */
    {
        const char *guess = closest_action(action, skill_actions, SKILL_ACTIONS);

        VLOG(1, "skill: unknown action '%s'%s",
             action ? action : "(null)",
             guess   ? "  (suggestion below)" : "");

        fprintf(stderr, "Unknown action '%s'.\n", action ? action : "(null)");
        if (guess)
            fprintf(stderr, "  Did you mean '%s'?\n", guess);
        fprintf(stderr, "  Run 'actagamma_db skill help' for full usage.\n");
        return EXIT_INVALID;
    }
}
