/* skill_cmd.c */
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
 *   acta_cli skill --help                                       */
void skill_usage(FILE *f)
{
    fputs(
"Usage: acta_cli skill <action> [options]\n"
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
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n"
"== create ===========================================================\n"
"  Create a new skill.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli skill create \\\n"
"      --name \"Summarize\" \\\n"
"      --prompt_template \"Summarize: {{input}}\" \\\n"
"      --folder_id 3 \\\n"
"      --description \"Summarises long text\" \\\n"
"      --output_schema '{\"type\":\"string\"}'\n"
"        <- flag-based\n"
"\n"
"    cat skill.json | acta_cli skill create --stdin\n"
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
"    --json <blob>          Read the skill as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose [N]        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single skill by its primary key.\n"
"\n"
"    acta_cli skill get 42\n"
"    acta_cli skill get 42 --include_deleted\n"
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
"  Update one or more fields on an existing skill.\n"
"  Unspecified fields are left unchanged.\n"
"\n"
"    acta_cli skill update 42 \\\n"
"      --name \"Summarize v2\" \\\n"
"      --prompt_template \"Summarize (v2): {{input}}\"\n"
"\n"
"    cat patch.json | acta_cli skill update 42 --json\n"
"        <- JSON patch via stdin (any subset of the fields)\n"
"\n"
"  At least one field is required:\n"
"    --name <str>             Display name\n"
"    --prompt_template <str>  Prompt template body\n"
"    --folder_id <int>        Owning folder (0 = root)\n"
"    --description <str>      Human-readable description\n"
"    --output_schema <json>   Expected output JSON schema\n"
"\n"
"  Note: a JSON body cannot move a skill back to the root folder\n"
"  (use: acta_cli skill move <id> --folder_id 0).\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the skill patch as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --verbose [N]        debug level 0-3 (stderr)\n"
"\n"
"== delete <id> ====================================================\n"
"  Soft-delete a skill (sets deleted_at; row is retained).\n"
"\n"
"    acta_cli skill delete 42\n"
"\n"
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted skill.\n"
"\n"
"    acta_cli skill restore 42\n"
"\n"
"== move <skill_id> ================================================\n"
"  Move a skill into (or out of) a folder.\n"
"\n"
"    acta_cli skill move 42 --folder_id 3\n"
"    acta_cli skill move 42 --folder_id 0   # back to root\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Destination folder (required, 0 = root)\n"
"\n"
"== list ===========================================================\n"
"  List skills, optionally filtered by folder.\n"
"\n"
"    acta_cli skill list\n"
"    acta_cli skill list --folder_id 3\n"
"    acta_cli skill list --all --offset 10 --limit 25\n"
"    acta_cli skill list --all --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Only skills in this folder\n"
"    --all                All folders — the default scope (no-op alone;\n"
"                         overrides --folder_id when combined)\n"
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
"  Count skills.\n"
"\n"
"    acta_cli skill count\n"
"    acta_cli skill count --folder_id 3\n"
"    acta_cli skill count --all\n"
"    acta_cli skill count --all --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Count only skills in this folder\n"
"    --all                All folders — the default scope (no-op alone;\n"
"                         overrides --folder_id when combined)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n"
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

static void usage_create(FILE *f)
{
    fputs(
"== create ===========================================================\n"
"  Create a new skill.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli skill create \\\n"
"      --name \"Summarize\" \\\n"
"      --prompt_template \"Summarize: {{input}}\" \\\n"
"      --folder_id 3 \\\n"
"      --description \"Summarises long text\" \\\n"
"      --output_schema '{\"type\":\"string\"}'\n"
"        <- flag-based\n"
"\n"
"    cat skill.json | acta_cli skill create --stdin\n"
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
"    --json <blob>          Read the skill as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose [N]        debug level 0-3 (stderr)\n", f);
}

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single skill by its primary key.\n"
"\n"
"    acta_cli skill get 42\n"
"    acta_cli skill get 42 --include_deleted\n"
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
"  Update one or more fields on an existing skill.\n"
"  Unspecified fields are left unchanged.\n"
"\n"
"    acta_cli skill update 42 \\\n"
"      --name \"Summarize v2\" \\\n"
"      --prompt_template \"Summarize (v2): {{input}}\"\n"
"\n"
"    cat patch.json | acta_cli skill update 42 --json\n"
"        <- JSON patch via stdin (any subset of the fields)\n"
"\n"
"  At least one field is required:\n"
"    --name <str>             Display name\n"
"    --prompt_template <str>  Prompt template body\n"
"    --folder_id <int>        Owning folder (0 = root)\n"
"    --description <str>      Human-readable description\n"
"    --output_schema <json>   Expected output JSON schema\n"
"\n"
"  Note: a JSON body cannot move a skill back to the root folder\n"
"  (use: acta_cli skill move <id> --folder_id 0).\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the skill patch as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --verbose [N]        debug level 0-3 (stderr)\n", f);
}

static void usage_delete(FILE *f)
{
    fputs(
"== delete <id> ====================================================\n"
"  Soft-delete a skill (sets deleted_at; row is retained).\n"
"\n"
"    acta_cli skill delete 42\n", f);
}

static void usage_restore(FILE *f)
{
    fputs(
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted skill.\n"
"\n"
"    acta_cli skill restore 42\n", f);
}

static void usage_move(FILE *f)
{
    fputs(
"== move <skill_id> ================================================\n"
"  Move a skill into (or out of) a folder.\n"
"\n"
"    acta_cli skill move 42 --folder_id 3\n"
"    acta_cli skill move 42 --folder_id 0   # back to root\n"
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
"    acta_cli skill list\n"
"    acta_cli skill list --folder_id 3\n"
"    acta_cli skill list --all --offset 10 --limit 25\n"
"    acta_cli skill list --all --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Only skills in this folder\n"
"    --all                All folders — the default scope (no-op alone;\n"
"                         overrides --folder_id when combined)\n"
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
"  Count skills.\n"
"\n"
"    acta_cli skill count\n"
"    acta_cli skill count --folder_id 3\n"
"    acta_cli skill count --all\n"
"    acta_cli skill count --all --include_deleted\n"
"\n"
"  Options:\n"
"    --folder_id <int>    Count only skills in this folder\n"
"    --all                All folders — the default scope (no-op alone;\n"
"                         overrides --folder_id when combined)\n"
"    --include_deleted    Include soft-deleted rows\n"
"    --deleted            Alias for --include_deleted\n", f);
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

/* free_row adapter for load_row_or_notfound (void* signature). */
static void skill_free_wrap(void *s)
{
    acta_db_skill_free((skill_t *)s);
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

/* P0: print the help section for one skill action.
 * 0 = printed, -1 = unknown action. */
int skill_help_for_action(const char *action, FILE *out)
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

int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    /* ── help: whole entity, or one action via `skill help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (skill_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("skill", sub, "acta_cli skill help",
                                  skill_actions, SKILL_ACTIONS);
        }
        skill_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        skill_t s = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_create(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "skill create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_skill(blob, &s) != 0) {
                VLOG(1, "  JSON parse error");
                emit_error("invalid JSON body");
                usage_create(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            const char *f_name    = cmd_args_flag(ga, "name", 1);
            const char *f_prompt  = cmd_args_flag(ga, "prompt_template", 1);
            const char *f_desc    = cmd_args_flag(ga, "description", 1);
            const char *f_schema  = cmd_args_flag(ga, "output_schema", 1);

            s.name            = (char *)f_name;
            s.prompt_template = (char *)f_prompt;
            s.description     = (char *)f_desc;
            s.output_schema   = (char *)f_schema;
            s.folder_id = 0;
            if (parse_nonneg_int_flag(ga, "folder_id", &s.folder_id, 0,
                                      usage_create, "skill create") < 0)
                return EXIT_INVALID;
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
            emit_error("missing required field: name");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_create;
        }
        if (!s.prompt_template || !*s.prompt_template) {
            VLOG(1, "  ERROR: missing required field 'prompt_template'");
            emit_error("missing required field: prompt_template");
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
            ret = finish_op_error(db, rc, "skill create");
            goto cleanup_skill_create;
        }

        VLOG(1, "  created skill id=%d", out_id);

        emit_ok_id(gopts, out_id);

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
        int id;
        if (!parse_id_positional(ga, "id", usage_get, "skill get", &id))
            return EXIT_INVALID;

        int f_inc_del = cmd_args_has_flag(ga, "include_deleted");

        VLOG(1, "skill get: fetching id=%d include_deleted=%d",
             id, f_inc_del);

        int err = 0;
        skill_t *s = f_inc_del
            ? acta_db_skill_get(db, id, &err)
            : acta_db_skill_get_live(db, id, &err);

        VLOG(3, "  acta_db_skill_get(_live)(%d) → ptr=%p err=%d",
             id, (const void *)s, err);

        int rc = load_row_or_notfound(db, err, s, id, skill_free_wrap,
                                      "skill get", "skill");
        if (rc)
            return rc;

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
        int id;
        if (!parse_id_positional(ga, "id", usage_update, "skill update", &id))
            return EXIT_INVALID;

        /* ── input flags (flag mode; JSON mode: --json/--stdin/--from_file) ── */
        const char *f_name    = cmd_args_flag(ga, "name", 1);
        const char *f_prompt  = cmd_args_flag(ga, "prompt_template", 1);
        const char *f_folder  = cmd_args_flag(ga, "folder_id", 1);
        const char *f_desc    = cmd_args_flag(ga, "description", 1);
        const char *f_schema  = cmd_args_flag(ga, "output_schema", 1);

        int has_folder = 0;   /* 1 once we've decided folder_id is being set */
        int folder_val = 0;   /* 0 == root (NULL) */
        int ret = EXIT_OK;

        if (!gopts->json_input && !gopts->from_stdin && !gopts->from_file) {
            /* At least one field must be provided for update. */
            if (!f_name && !f_prompt && !f_desc && !f_schema && !f_folder) {
                VLOG(1, "  ERROR: no fields provided for update");
                emit_error("at least one field required for update");
                usage_update(stderr);
                return EXIT_INVALID;
            }
            /* Hand-rolled (not require_flag): the atom's message
             * ("field 'name' must not be empty") differs from this
             * entity's contract ("--name must not be empty"). */
            if (f_name && !*f_name) {
                VLOG(1, "  ERROR: --name must not be empty");
                emit_error("--name must not be empty");
                usage_update(stderr);
                return EXIT_INVALID;
            }
            if (f_folder) {
                if (parse_nonneg_int_flag(ga, "folder_id", &folder_val, 0,
                                          usage_update, "skill update") < 0)
                    return EXIT_INVALID;
                has_folder = 1;
            }
        }

        /*
         * Fetch the current row so we can fill in any fields the caller
         * did not supply (partial-update → full-update merge).
         * Done before reading stdin so a missing id fails fast.
         */
        int err = 0;
        skill_t *cur = acta_db_skill_get_live(db, id, &err);
        {
            int rc_fetch = load_row_or_notfound(db, err, cur, id,
                                                skill_free_wrap,
                                                "skill update", "skill");
            if (rc_fetch)
                return rc_fetch;
        }

        /* Shallow-merge: start from the live row, override only what
         * the caller actually provided.  String pointers either point
         * into `cur` (freed with cur) or into `parsed`'s blob buffers
         * (freed separately below); argv strings are owned by main. */
        skill_t s = *cur;
        skill_t parsed = {0};
        int parsed_owned = 0;

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_update(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_update;
        }
        if (src) {
            VLOG(1, "skill update: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_skill(blob, &parsed) != 0) {
                VLOG(1, "  JSON parse error");
                emit_error("invalid JSON body");
                usage_update(stderr);
                free(blob);
                ret = EXIT_INVALID;
                goto cleanup_skill_update;
            }
            free(blob);
            parsed_owned = 1;

            /* positional <id> is the authority */
            parsed.id = id;

            /* At least one recognised field must be present. */
            if (!parsed.name && !parsed.prompt_template &&
                !parsed.description && !parsed.output_schema &&
                parsed.folder_id <= 0) {
                VLOG(1, "  ERROR: no fields provided in JSON body");
                emit_error("at least one field required in JSON body");
                usage_update(stderr);
                ret = EXIT_INVALID;
                goto cleanup_skill_update;
            }

            /* patch: adopt only the fields present in the blob */
            if (parsed.name)            s.name            = parsed.name;
            if (parsed.prompt_template) s.prompt_template = parsed.prompt_template;
            if (parsed.description)     s.description     = parsed.description;
            if (parsed.output_schema)   s.output_schema   = parsed.output_schema;
            /* folder_id == 0 means "absent" as well as "root", so a
             * JSON body cannot move the skill back to the root folder
             * (use: skill move <id> --folder_id 0). */
            if (parsed.folder_id > 0)   s.folder_id       = parsed.folder_id;
        } else {
            if (f_name)        s.name            = (char *)f_name;
            if (f_prompt)      s.prompt_template = (char *)f_prompt;
            if (f_desc)        s.description     = (char *)f_desc;
            if (f_schema)      s.output_schema   = (char *)f_schema;
            if (has_folder)    s.folder_id       = folder_val;
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

        VLOG(3, "  raw: ga=%p parsed_owned=%d s=%p name=%p prompt=%p "
                "folder=%d desc=%p schema=%p",
             (const void *)ga, parsed_owned, (const void *)&s,
             (const void *)s.name,
             (const void *)s.prompt_template,
             s.folder_id,
             (const void *)s.description,
             (const void *)s.output_schema);

        /* ── post-merge validation ──────────────────────────────────
         * The live row always carries a non-empty name/prompt (create
         * rejects empty), so these only fire when the caller passed an
         * explicitly empty string (e.g. JSON "name":""). */
        if (!s.name || !*s.name) {
            VLOG(1, "  ERROR: 'name' is empty");
            emit_error("name must not be empty");
            usage_update(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_update;
        }
        if (!s.prompt_template || !*s.prompt_template) {
            VLOG(1, "  ERROR: 'prompt_template' is empty");
            emit_error("prompt_template must not be empty");
            usage_update(stderr);
            ret = EXIT_INVALID;
            goto cleanup_skill_update;
        }

        vlog_skill_fields("  pre-update", &s);

        int rc = acta_db_skill_update(db, &s);

        VLOG(3, "  acta_db_skill_update → rc=%d", rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = finish_op_error(db, rc, "skill update");
            goto cleanup_skill_update;
        }

        VLOG(1, "  updated skill id=%d", id);
        emit_ok_id(gopts, id);

        ret = EXIT_OK;
        goto cleanup_skill_update;

    cleanup_skill_update:
        /* `s` shares string pointers with `cur` (live row) and/or
         * `parsed` (blob); free each allocation exactly once. */
        acta_db_skill_free(cur);
        if (parsed_owned) {
            free(parsed.name);
            free(parsed.description);
            free(parsed.prompt_template);
            free(parsed.output_schema);
            free(parsed.created_at);
            free(parsed.updated_at);
            free(parsed.deleted_at);
        }
        return ret;
    }


    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_delete, "skill delete", &id))
            return EXIT_INVALID;

        VLOG(1, "skill delete: id=%d", id);

        int rc = acta_db_skill_soft_delete(db, id);

        VLOG(3, "  acta_db_skill_soft_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill delete");
        }

        VLOG(1, "  deleted skill id=%d", id);
        emit_deleted();
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_restore, "skill restore", &id))
            return EXIT_INVALID;

        VLOG(1, "skill restore: id=%d", id);

        int rc = acta_db_skill_restore(db, id);

        VLOG(3, "  acta_db_skill_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill restore");
        }

        VLOG(1, "  restored skill id=%d", id);
        emit_ok_restored(gopts, id);
        return EXIT_OK;
    }

    /* ── move <skill_id> --folder_id <folder_id> ──────────────────── */
    if (strcmp(action, "move") == 0) {
        int skill_id;
        if (!parse_id_positional(ga, "skill_id", usage_move, "skill move",
                                 &skill_id))
            return EXIT_INVALID;

        int folder_id = 0;
        if (parse_nonneg_int_flag(ga, "folder_id", &folder_id, 1,
                                  usage_move, "skill move") < 0)
            return EXIT_INVALID;

        VLOG(1, "skill move: skill_id=%d folder_id=%d", skill_id, folder_id);

        VLOG(2, "  params: skill_id=%d folder_id=%d", skill_id, folder_id);

        VLOG(3, "  raw: ga=%p skill_id=%d folder_id=%d",
             (const void *)ga, skill_id, folder_id);

        int rc = acta_db_skill_move_to_folder(db, skill_id, folder_id);

        VLOG(3, "  acta_db_skill_move_to_folder(%d, %d) → rc=%d",
             skill_id, folder_id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "skill move");
        }

        VLOG(1, "  moved skill id=%d → folder_id=%d", skill_id, folder_id);
        emit_ok_folder(gopts, skill_id, folder_id);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        /* KI-6: --id_only is a single-row modifier (create/get); list
         * actions reject it with exit 4 instead of silently ignoring it. */
        if (gopts->id_only) {
            emit_error("skill list: --id_only is not supported; remove the "
                       "flag (use the JSON rows, --count, --table, or "
                       "--stream)");
            return EXIT_INVALID;
        }

        const char *f_folder = cmd_args_flag(ga, "folder_id", 1); /* VLOG display only */
        int has_all = cmd_args_has_flag(ga, "all");
        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        int offset = 0, limit = 0;
        int folder_id = 0;
        int r_folder = parse_nonneg_int_flag(ga, "folder_id", &folder_id, 0,
                                             usage_list, "skill list");
        if (r_folder < 0)
            return EXIT_INVALID;
        int in_folder = r_folder;

        if (parse_offset_limit(ga, &offset, &limit,
                               usage_list, "skill list") < 0)
            return EXIT_INVALID;

        VLOG(1, "skill list: folder_id=%s all=%d offset=%d limit=%d "
                "include_deleted=%d",
             f_folder ? f_folder : (has_all ? "(any/all)" : "(root)"),
             has_all ? 1 : 0,
             offset, limit, include_deleted);

        VLOG(2, "  full: folder_id=%s all=%d offset=%d limit=%d "
                "include_deleted=%d no_nulls=%d table=%d fields=%s",
             f_folder ? f_folder : "(null)",
             has_all ? 1 : 0,
             offset, limit, include_deleted,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  folder_id=%d all=%d offset=%d limit=%d include_deleted=%d",
             folder_id, has_all ? 1 : 0, offset, limit, include_deleted);

        if (gopts->count) {
            int err = 0;
            int n;
            if (in_folder && !has_all)
                n = include_deleted
                    ? acta_db_skill_count_in_folder_with_deleted(
                          db, folder_id, &err)
                    : acta_db_skill_count_in_folder(db, folder_id, &err);
            else
                n = include_deleted
                    ? acta_db_skill_count_all_with_deleted(db, &err)
                    : acta_db_skill_count_all(db, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "skill count");
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
                skill_usage(stderr);
                return EXIT_INVALID;
            }
            int emitted = 0;
            for (;;) {
                int want = (limit > 0) ? limit - emitted : 0;
                int o = offset + emitted;
                int n = 0, e2 = 0;
                skill_t **items;
                if (in_folder && !has_all) {
                    items = include_deleted
                        ? acta_db_skill_list_in_folder_with_deleted(
                              db, folder_id, o, want, &n, &e2)
                        : acta_db_skill_list_in_folder(db, folder_id,
                                                       o, want, &n, &e2);
                } else {
                    items = include_deleted
                        ? acta_db_skill_list_all_with_deleted(db, o, want,
                                                              &n, &e2)
                        : acta_db_skill_list_all(db, o, want, &n, &e2);
                }
                if (e2 != ACTA_DB_OK) {
                    acta_db_skill_list_free(items, n);
                    return finish_op_error(db, e2, "skill list");
                }
                for (int i = 0; i < n; i++) {
                    skill_to_json(stdout, items[i], gopts);
                    fputc('\n', stdout);
                }
                acta_db_skill_list_free(items, n);
                emitted += n;
                if ((limit > 0 && emitted >= limit) || n == 0 || n < want)
                    break;
            }
            VLOG(1, "  stream: %d item(s) emitted", emitted);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        skill_t **items;

        if (in_folder && !has_all) {
            items = include_deleted
                ? acta_db_skill_list_in_folder_with_deleted(
                      db, folder_id, offset, limit, &out_count, &err)
                : acta_db_skill_list_in_folder(db, folder_id,
                                               offset, limit,
                                               &out_count, &err);
        } else {
            items = include_deleted
                ? acta_db_skill_list_all_with_deleted(
                      db, offset, limit, &out_count, &err)
                : acta_db_skill_list_all(db, offset, limit,
                                         &out_count, &err);
        }

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_skill_list_free(items, out_count);
            return finish_op_error(db, err, "skill list");
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
        int r_folder = parse_nonneg_int_flag(ga, "folder_id", &folder_id, 0,
                                             usage_count, "skill count");
        if (r_folder < 0)
            return EXIT_INVALID;
        int in_folder = r_folder;
        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        VLOG(1, "skill count: folder_id=%s all=%d include_deleted=%d",
             f_folder ? f_folder : (has_all ? "(any/all)" : "(root)"),
             has_all ? 1 : 0, include_deleted);

        VLOG(2, "  folder_id=%d all=%d include_deleted=%d",
             folder_id, has_all ? 1 : 0, include_deleted);

        int err = 0;
        int n;
        if (in_folder && !has_all)
            n = include_deleted
                ? acta_db_skill_count_in_folder_with_deleted(
                      db, folder_id, &err)
                : acta_db_skill_count_in_folder(db, folder_id, &err);
        else
            n = include_deleted
                ? acta_db_skill_count_all_with_deleted(db, &err)
                : acta_db_skill_count_all(db, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "skill count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("skill", action, "acta_cli skill help",
                          skill_actions, SKILL_ACTIONS);
}
