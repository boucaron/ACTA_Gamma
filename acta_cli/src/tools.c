/*
 * tools.c — `--tools` machine-readable tool schema (T3).
 *
 * Single source of truth: a static per-(entity, action) data table,
 * rendered as one JSON object with a global contract section and a
 * `tools` array.  Data is generated from docs/cli_spec.md (T1, with the
 * T3 M1–M3 spec-table fixes applied):
 *   M1  context create JSON key is `hash`, not `content_hash`
 *   M2  skill update takes flags OR JSON (>= 1 field)
 *   M3  skill_folder list/count filter by an optional positional
 *       <parent_id | all>, not a --parent_id flag
 *
 * Flag vocabulary: every flag listed per action is a subset of
 * entity_flag_specs (argparse.c), so commands built from this table
 * pass cmd_args_validate.  Global flags (--db, --fields, ..., --pretty)
 * live only in the global section — parse_globals consumes them before
 * the entity/action dispatch.
 *
 * No DB access: `--tools` early-exits in main.c before the database is
 * opened, so the whole object is static data.  Default rendering is
 * single-line compact (script-friendly, one JSON value); --pretty
 * switches to 2-space indent.  A trailing newline is emitted in both
 * modes.
 */

#include <stdio.h>

#include "cli.h"      /* EXIT_OK */
#include "cli_util.h" /* json_str */

/* ------------------------------------------------------------------ */
/*  schema data                                                        */
/* ------------------------------------------------------------------ */

typedef struct { const char *name; int required; const char *type; } tool_pos_t;
typedef struct { const char *name; int has_value; int required; } tool_flag_t;

typedef struct {
    const char *command;   /* "<entity>.<action>" */
    const char *entity;
    const char *action;
    const char *const *aliases;  size_t n_aliases;
    const char *description;
    const tool_pos_t *positionals; size_t n_pos;
    const tool_flag_t *flags;     size_t n_flags;
    const char *input;   /* "flags" | "flags|json" | "positional|flags"
                           | "positional" | "none" */
    const char *const *json_req;  size_t n_json_req;
    const char *const *json_opt;  size_t n_json_opt;
    const char *success;  /* stdout on success, cli_spec.md verbatim */
} tool_entry_t;

/* ── positionals ── */

static const tool_pos_t p_id[]        = { { "id", 1, "positive-int" } };
static const tool_pos_t p_model_id[]  = { { "model_id", 1, "positive-int" } };
static const tool_pos_t p_skill_id[]  = { { "skill_id", 1, "positive-int" } };
static const tool_pos_t p_exec_id[]   = { { "execution_id", 1, "positive-int" } };
static const tool_pos_t p_sql[]       = { { "sql", 0, "string" } };
static const tool_pos_t p_sf_parent[] =
    { { "parent_id", 0, "non-negative-int, or the sentinel 'all'" } };

/* ── per-action flags (subset of entity_flag_specs) ── */

static const tool_flag_t f_db_exec[] = {
    { "sql", 1, 0 }, { "file", 1, 0 }, { "sql_stdin", 0, 0 },
};
static const tool_flag_t f_table[]   = { { "table", 0, 0 } };

static const tool_flag_t f_ctx_create[] = {
    { "type", 1, 1 }, { "content", 1, 1 },
    { "hash", 1, 0 }, { "metadata", 1, 0 },
};
static const tool_flag_t f_ctx_list[] = {
    { "type", 1, 0 }, { "hash", 1, 0 },
    { "offset", 1, 0 }, { "limit", 1, 0 },
    { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};
static const tool_flag_t f_ctx_count[] = {
    { "type", 1, 0 }, { "hash", 1, 0 },
};

static const tool_flag_t f_model_create[] = {
    { "name", 1, 1 }, { "backend", 1, 1 }, { "model_identifier", 1, 1 },
    { "folder_id", 1, 0 }, { "description", 1, 0 },
    { "base_url", 1, 0 }, { "configuration", 1, 0 },
};
static const tool_flag_t f_inc_del[]   = { { "include_deleted", 0, 0 } };
static const tool_flag_t f_model_update[] = {
    { "name", 1, 0 }, { "folder_id", 1, 0 }, { "description", 1, 0 },
    { "backend", 1, 0 }, { "base_url", 1, 0 },
    { "model_identifier", 1, 0 }, { "configuration", 1, 0 },
};
static const tool_flag_t f_folder_id[] = { { "folder_id", 1, 1 } };
static const tool_flag_t f_model_list[] = {
    { "folder_id", 1, 0 }, { "offset", 1, 0 }, { "limit", 1, 0 },
    { "include_deleted", 0, 0 }, { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};
static const tool_flag_t f_model_count[] = {
    { "folder_id", 1, 0 }, { "include_deleted", 0, 0 },
};

static const tool_flag_t f_folder_create[] = {
    { "name", 1, 1 }, { "parent_id", 1, 0 },
};
static const tool_flag_t f_mf_list[] = {
    { "parent_id", 1, 0 }, { "offset", 1, 0 }, { "limit", 1, 0 },
    { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};
static const tool_flag_t f_parent_id[]   = { { "parent_id", 1, 0 } };
static const tool_flag_t f_name[]        = { { "name", 1, 1 } };
static const tool_flag_t f_parent_id_req[] = { { "parent_id", 1, 1 } };

/* Documented dispatch aliases, repeated per-entry so the schema literally
 * says `execution <action>` / `execution_log <action>` are not commands. */
static const char *const alias_execution[]    = { "execution" };
static const char *const alias_execution_log[] = { "execution_log" };

static const tool_flag_t f_rev_list[] = {
    { "offset", 1, 0 }, { "limit", 1, 0 },
    { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};

static const tool_flag_t f_skill_create[] = {
    { "name", 1, 1 }, { "prompt_template", 1, 1 },
    { "folder_id", 1, 0 }, { "description", 1, 0 }, { "output_schema", 1, 0 },
};
static const tool_flag_t f_skill_update[] = {
    { "name", 1, 0 }, { "folder_id", 1, 0 }, { "description", 1, 0 },
    { "prompt_template", 1, 0 }, { "output_schema", 1, 0 },
};
static const tool_flag_t f_skill_list[] = {
    { "all", 0, 0 }, { "folder_id", 1, 0 }, { "offset", 1, 0 }, { "limit", 1, 0 },
    { "include_deleted", 0, 0 }, { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};
static const tool_flag_t f_skill_count[] = {
    { "all", 0, 0 }, { "folder_id", 1, 0 }, { "include_deleted", 0, 0 },
};

static const tool_flag_t f_sf_list[] = {
    { "offset", 1, 0 }, { "limit", 1, 0 },
    { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};

static const tool_flag_t f_exec_create[] = {
    { "context_id", 1, 1 }, { "skill_revision_id", 1, 1 },
    { "model_revision_id", 1, 1 },
    { "prompt", 1, 0 }, { "parent_execution_id", 1, 0 },
};
static const tool_flag_t f_exec_complete[] = { { "result", 0, 0 } };
static const tool_flag_t f_exec_fail[]     = { { "error", 0, 0 } };
static const tool_flag_t f_raw[]           = { { "raw", 1, 1 } };
static const tool_flag_t f_exec_list[] = {
    { "status", 1, 0 }, { "context_id", 1, 0 },
    { "skill_revision_id", 1, 0 }, { "model_revision_id", 1, 0 },
    { "parent_execution_id", 1, 0 },
    { "offset", 1, 0 }, { "limit", 1, 0 },
    { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};
static const tool_flag_t f_exec_count[] = {
    { "status", 1, 0 }, { "context_id", 1, 0 },
    { "skill_revision_id", 1, 0 }, { "model_revision_id", 1, 0 },
    { "parent_execution_id", 1, 0 },
};

static const tool_flag_t f_log_create[] = {
    { "execution_id", 1, 1 }, { "level", 1, 1 }, { "event", 1, 1 },
    { "message", 1, 0 }, { "metadata", 1, 0 },
};
static const tool_flag_t f_level[]  = { { "level", 1, 0 } };
static const tool_flag_t f_log_list[] = {
    { "level", 1, 0 }, { "offset", 1, 0 }, { "limit", 1, 0 },
    { "count", 0, 0 }, { "table", 0, 0 },
    { "fields", 1, 0 }, { "no_nulls", 0, 0 },
};

/* ── JSON-body keys (from json.c parsers; M1/M2 applied) ── */

static const char *const jk_ctx_req[]   = { "type", "content", "hash" };
static const char *const jk_ctx_opt[]   = { "metadata" };
static const char *const jk_model_req[] = { "name", "backend", "model_identifier" };
static const char *const jk_model_opt[] =
    { "folder_id", "description", "base_url", "configuration" };
static const char *const jk_folder_req[] = { "name" };
static const char *const jk_folder_opt[] = { "parent_id" };
static const char *const jk_skill_req[]  = { "name", "prompt_template" };
static const char *const jk_skill_opt[]  =
    { "folder_id", "description", "output_schema" };
static const char *const jk_skill_upd[]  =
    { "name", "prompt_template", "folder_id", "description", "output_schema" };
static const char *const jk_exec_req[]   =
    { "context_id", "skill_revision_id", "model_revision_id" };
static const char *const jk_exec_opt[]   = { "prompt", "parent_execution_id" };
static const char *const jk_log_req[]    = { "execution_id", "level", "event" };
static const char *const jk_log_opt[]    = { "message", "metadata" };

/* ── global section data ── */

static const tool_flag_t global_flags[] = {
    { "db", 1, 0 }, { "fields", 1, 0 }, { "no_nulls", 0, 0 },
    { "id_only", 0, 0 }, { "count", 0, 0 }, { "table", 0, 0 },
    { "pretty", 0, 0 }, { "json", 1, 0 }, { "stdin", 0, 0 },
    { "from_file", 1, 0 }, { "version", 0, 0 }, { "help", 0, 0 },
    { "tools", 0, 0 }, { "verbose", 0, 0 },
};

static const char *const input_sources[] = { "json", "stdin", "from_file" };

typedef struct { const char *code; const char *meaning; } exit_code_t;

static const exit_code_t exit_codes[] = {
    { "0",  "ok" },
    { "1",  "not found" },
    { "2",  "SQL error" },
    { "3",  "OOM" },
    { "4",  "invalid argument / missing flag / missing required field / "
            "duplicate / FK violation / invalid DB file" },
    { "10", "CLI usage error (unknown entity, unknown action, unknown "
            "option, bad --verbose, too few positionals, missing flag value)" },
    { "11", "DB open failed" },
};

/* ------------------------------------------------------------------ */
/*  the table: 69 entries = 59 actions + 10 help actions              */
/* ------------------------------------------------------------------ */

static const tool_entry_t tool_table[] = {
    /* ── db ── */
    { "db.exec", "db", "exec", NULL, 0,
      "Execute mutating SQL (no SELECT). Exactly one source, mutually "
      "exclusive, first wins in order: positional sql, --sql, --file, "
      "--sql_stdin; --file and --sql_stdin are limited to 64 KiB. Never "
      "JSON; the global --stdin is rejected (use --sql_stdin).",
      p_sql, 1, f_db_exec, 3, "positional|flags",
      NULL, 0, NULL, 0,
      "{\"status\":\"ok\"} (--table -> ok)" },

    { "db.version", "db", "version", NULL, 0,
      "Print the SQLite library version.",
      NULL, 0, f_table, 1, "none",
      NULL, 0, NULL, 0,
      "{\"version\":\"<ver>\"} (--table -> SQLite <ver>)" },

    { "db.help", "db", "help", NULL, 0,
      "Show the db usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── context ── */
    { "context.create", "context", "create", NULL, 0,
      "Create a context from flags or a JSON body.",
      NULL, 0, f_ctx_create, 4, "flags|json",
      jk_ctx_req, 3, jk_ctx_opt, 1,
      "{\"id\":N}" },

    { "context.get", "context", "get", NULL, 0,
      "Fetch a context by id.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "context JSON object" },

    { "context.list", "context", "list", NULL, 0,
      "List contexts, optionally filtered by type and hash.",
      NULL, 0, f_ctx_list, 8, "flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "context.count", "context", "count", NULL, 0,
      "Count contexts, optionally filtered by type and hash.",
      NULL, 0, f_ctx_count, 2, "flags",
      NULL, 0, NULL, 0,
      "bare int" },

    { "context.help", "context", "help", NULL, 0,
      "Show the context usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── model ── */
    { "model.create", "model", "create", NULL, 0,
      "Create a model from flags or a JSON body.",
      NULL, 0, f_model_create, 7, "flags|json",
      jk_model_req, 3, jk_model_opt, 4,
      "{\"id\":N}" },

    { "model.get", "model", "get", NULL, 0,
      "Fetch a model by id (--include_deleted, alias --deleted, returns "
      "soft-deleted rows).",
      p_id, 1, f_inc_del, 1, "positional",
      NULL, 0, NULL, 0,
      "model JSON object" },

    { "model.update", "model", "update", NULL, 0,
      "Update one or more model fields; at least one of the listed flags "
      "is required. Flags only (no JSON input).",
      p_id, 1, f_model_update, 7, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N}" },

    { "model.delete", "model", "delete", NULL, 0,
      "Soft-delete a model.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"deleted\":true}" },

    { "model.restore", "model", "restore", NULL, 0,
      "Restore a soft-deleted model.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"restored\":true}" },

    { "model.move", "model", "move", NULL, 0,
      "Move a model to a folder (0 = root).",
      p_id, 1, f_folder_id, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"folder_id\":null|M}" },

    { "model.list", "model", "list", NULL, 0,
      "List models, optionally filtered by folder.",
      NULL, 0, f_model_list, 8, "flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "model.count", "model", "count", NULL, 0,
      "Count models, optionally filtered by folder.",
      NULL, 0, f_model_count, 2, "flags",
      NULL, 0, NULL, 0,
      "bare int" },

    { "model.help", "model", "help", NULL, 0,
      "Show the model usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── model_folder ── */
    { "model_folder.create", "model_folder", "create", NULL, 0,
      "Create a model folder from flags or a JSON body "
      "(parent_id 0 or omitted = root).",
      NULL, 0, f_folder_create, 2, "flags|json",
      jk_folder_req, 1, jk_folder_opt, 1,
      "{\"id\":N}" },

    { "model_folder.get", "model_folder", "get", NULL, 0,
      "Fetch a model folder by id.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "folder JSON object" },

    { "model_folder.list", "model_folder", "list", NULL, 0,
      "List model folders, optionally filtered by --parent_id.",
      NULL, 0, f_mf_list, 7, "flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "model_folder.count", "model_folder", "count", NULL, 0,
      "Count model folders, optionally filtered by --parent_id.",
      NULL, 0, f_parent_id, 1, "flags",
      NULL, 0, NULL, 0,
      "bare int" },

    { "model_folder.rename", "model_folder", "rename", NULL, 0,
      "Rename a model folder.",
      p_id, 1, f_name, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N}" },

    { "model_folder.delete", "model_folder", "delete", NULL, 0,
      "Soft-delete a model folder.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"deleted\":true}" },

    { "model_folder.restore", "model_folder", "restore", NULL, 0,
      "Restore a soft-deleted model folder.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"restored\":true}" },

    { "model_folder.move", "model_folder", "move", NULL, 0,
      "Move a model folder to a parent (0 = root).",
      p_id, 1, f_parent_id_req, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"parent_id\":null|M}" },

    { "model_folder.help", "model_folder", "help", NULL, 0,
      "Show the model_folder usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── model_revision ── */
    { "model_revision.get", "model_revision", "get", NULL, 0,
      "Fetch a model revision by id.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "revision JSON object" },

    { "model_revision.get-latest", "model_revision", "get-latest", NULL, 0,
      "Fetch the latest revision of a model.",
      p_model_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "revision JSON object" },

    { "model_revision.list", "model_revision", "list", NULL, 0,
      "List revisions of a model.",
      p_model_id, 1, f_rev_list, 6, "positional|flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "model_revision.count", "model_revision", "count", NULL, 0,
      "Count revisions of a model.",
      p_model_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "bare int" },

    { "model_revision.help", "model_revision", "help", NULL, 0,
      "Show the model_revision usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── skill ── */
    { "skill.create", "skill", "create", NULL, 0,
      "Create a skill from flags or a JSON body.",
      NULL, 0, f_skill_create, 5, "flags|json",
      jk_skill_req, 2, jk_skill_opt, 3,
      "{\"id\":N}" },

    { "skill.get", "skill", "get", NULL, 0,
      "Fetch a skill by id (--include_deleted, alias --deleted, returns "
      "soft-deleted rows).",
      p_id, 1, f_inc_del, 1, "positional",
      NULL, 0, NULL, 0,
      "skill JSON object" },

    { "skill.update", "skill", "update", NULL, 0,
      "Update one or more skill fields; at least one field is required "
      "(flags or JSON). JSON cannot move a skill back to the root folder "
      "(use 'skill move <id> --folder_id 0').",
      p_id, 1, f_skill_update, 5, "flags|json",
      NULL, 0, jk_skill_upd, 5,
      "{\"id\":N}" },

    { "skill.delete", "skill", "delete", NULL, 0,
      "Soft-delete a skill.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"deleted\":true}" },

    { "skill.restore", "skill", "restore", NULL, 0,
      "Restore a soft-deleted skill.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"restored\":true}" },

    { "skill.move", "skill", "move", NULL, 0,
      "Move a skill to a folder (0 = root).",
      p_id, 1, f_folder_id, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"folder_id\":null|M}" },

    { "skill.list", "skill", "list", NULL, 0,
      "List skills, optionally filtered by folder.",
      NULL, 0, f_skill_list, 9, "flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "skill.count", "skill", "count", NULL, 0,
      "Count skills, optionally filtered by folder.",
      NULL, 0, f_skill_count, 3, "flags",
      NULL, 0, NULL, 0,
      "bare int" },

    { "skill.help", "skill", "help", NULL, 0,
      "Show the skill usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── skill_folder ── */
    { "skill_folder.create", "skill_folder", "create", NULL, 0,
      "Create a skill folder from flags or a JSON body "
      "(parent_id 0 or omitted = root).",
      NULL, 0, f_folder_create, 2, "flags|json",
      jk_folder_req, 1, jk_folder_opt, 1,
      "{\"id\":N}" },

    { "skill_folder.get", "skill_folder", "get", NULL, 0,
      "Fetch a skill folder by id.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "folder JSON object" },

    { "skill_folder.list", "skill_folder", "list", NULL, 0,
      "List skill folders; the parent filter is the optional positional "
      "<parent_id> ('all' = all folders), not a --parent_id flag.",
      p_sf_parent, 1, f_sf_list, 6, "positional|flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "skill_folder.count", "skill_folder", "count", NULL, 0,
      "Count skill folders; the parent filter is the optional positional "
      "<parent_id> ('all' = all folders), not a --parent_id flag.",
      p_sf_parent, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "bare int" },

    { "skill_folder.rename", "skill_folder", "rename", NULL, 0,
      "Rename a skill folder.",
      p_id, 1, f_name, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N}" },

    { "skill_folder.delete", "skill_folder", "delete", NULL, 0,
      "Soft-delete a skill folder.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"deleted\":true}" },

    { "skill_folder.restore", "skill_folder", "restore", NULL, 0,
      "Restore a soft-deleted skill folder.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"restored\":true}" },

    { "skill_folder.move", "skill_folder", "move", NULL, 0,
      "Move a skill folder to a parent (0 = root).",
      p_id, 1, f_parent_id, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"parent_id\":null|M}" },

    { "skill_folder.help", "skill_folder", "help", NULL, 0,
      "Show the skill_folder usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── skill_revision ── */
    { "skill_revision.get", "skill_revision", "get", NULL, 0,
      "Fetch a skill revision by id.",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "revision JSON object" },

    { "skill_revision.get-latest", "skill_revision", "get-latest", NULL, 0,
      "Fetch the latest revision of a skill.",
      p_skill_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "revision JSON object" },

    { "skill_revision.list", "skill_revision", "list", NULL, 0,
      "List revisions of a skill.",
      p_skill_id, 1, f_rev_list, 6, "positional|flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "skill_revision.count", "skill_revision", "count", NULL, 0,
      "Count revisions of a skill.",
      p_skill_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "bare int" },

    { "skill_revision.help", "skill_revision", "help", NULL, 0,
      "Show the skill_revision usage text.",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── exec ── */
    { "exec.create", "exec", "create", alias_execution, 1,
      "Create an execution from flags or a JSON body. The row is always "
      "created 'pending'; --status (flag or JSON key) is rejected — later "
      "states are reached via start/complete/fail/cancel. The canonical "
      "entity name is 'exec' (dispatch rejects the alias 'execution').",
      NULL, 0, f_exec_create, 5, "flags|json",
      jk_exec_req, 3, jk_exec_opt, 2,
      "{\"id\":N} (row always created 'pending')" },

    { "exec.get", "exec", "get", alias_execution, 1,
      "Fetch an execution by id. (Canonical entity name is 'exec'; "
      "dispatch rejects the alias 'execution'.)",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "execution JSON object" },

    { "exec.start", "exec", "start", alias_execution, 1,
      "Start an execution (pending -> running). (Canonical entity name is "
      "'exec'; dispatch rejects the alias 'execution'.)",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"status\":\"running\"}" },

    { "exec.cancel", "exec", "cancel", alias_execution, 1,
      "Cancel an execution. (Canonical entity name is 'exec'; dispatch "
      "rejects the alias 'execution'.)",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"status\":\"cancelled\"}" },

    { "exec.complete", "exec", "complete", alias_execution, 1,
      "Complete an execution. (Canonical entity name is 'exec'; dispatch "
      "rejects the alias 'execution'.)",
      p_id, 1, f_exec_complete, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"status\":\"completed\"}" },

    { "exec.fail", "exec", "fail", alias_execution, 1,
      "Fail an execution. (Canonical entity name is 'exec'; dispatch "
      "rejects the alias 'execution'.)",
      p_id, 1, f_exec_fail, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"status\":\"failed\"}" },

    { "exec.set-raw", "exec", "set-raw", alias_execution, 1,
      "Set the raw model response on an execution. Status is unchanged; "
      "the success line echoes the current status. (Canonical entity name "
      "is 'exec'; dispatch rejects the alias 'execution'.)",
      p_id, 1, f_raw, 1, "flags",
      NULL, 0, NULL, 0,
      "{\"id\":N,\"status\":\"<current status, unchanged>\"}" },

    { "exec.list", "exec", "list", alias_execution, 1,
      "List executions, optionally filtered by status and refs. (Canonical "
      "entity name is 'exec'; dispatch rejects the alias 'execution'.)",
      NULL, 0, f_exec_list, 11, "flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "exec.count", "exec", "count", alias_execution, 1,
      "Count executions, optionally filtered by status and refs. "
      "(Canonical entity name is 'exec'; dispatch rejects the alias "
      "'execution'.)",
      NULL, 0, f_exec_count, 5, "flags",
      NULL, 0, NULL, 0,
      "bare int" },

    { "exec.help", "exec", "help", alias_execution, 1,
      "Show the exec usage text. (Canonical entity name is 'exec'; "
      "dispatch rejects the alias 'execution'.)",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },

    /* ── log ── */
    { "log.create", "log", "create", alias_execution_log, 1,
      "Create an execution log entry from flags or a JSON body. level "
      "must be one of: debug, info, warn, error. The canonical entity name "
      "is 'log' (dispatch rejects the alias 'execution_log').",
      NULL, 0, f_log_create, 5, "flags|json",
      jk_log_req, 3, jk_log_opt, 2,
      "{\"id\":N}" },

    { "log.get", "log", "get", alias_execution_log, 1,
      "Fetch a log entry by id. (Canonical entity name is 'log'; dispatch "
      "rejects the alias 'execution_log'.)",
      p_id, 1, NULL, 0, "positional",
      NULL, 0, NULL, 0,
      "log JSON object" },

    { "log.list", "log", "list", alias_execution_log, 1,
      "List log entries of an execution, optionally filtered by level. "
      "(Canonical entity name is 'log'; dispatch rejects the alias "
      "'execution_log'.)",
      p_exec_id, 1, f_log_list, 7, "positional|flags",
      NULL, 0, NULL, 0,
      "[ ... ] / []; --count -> bare int" },

    { "log.count", "log", "count", alias_execution_log, 1,
      "Count log entries of an execution, optionally filtered by level. "
      "(Canonical entity name is 'log'; dispatch rejects the alias "
      "'execution_log'.)",
      p_exec_id, 1, f_level, 1, "positional|flags",
      NULL, 0, NULL, 0,
      "bare int" },

    { "log.help", "log", "help", alias_execution_log, 1,
      "Show the log usage text. (Canonical entity name is 'log'; dispatch "
      "rejects the alias 'execution_log'.)",
      NULL, 0, NULL, 0, "none",
      NULL, 0, NULL, 0,
      "usage text (plain, not JSON)" },
};

#define TOOL_COUNT (sizeof(tool_table) / sizeof(tool_table[0]))

/* ------------------------------------------------------------------ */
/*  rendering (hand-rolled, matches the existing json_str emitter)     */
/* ------------------------------------------------------------------ */

static void indent_line(FILE *f, int levels)
{
    for (int k = 0; k < levels; k++) fputs("  ", f);
}

static void js(FILE *f, const char *s)
{
    json_str(f, s);
}

/* "key": separator — both modes: optional leading comma before fields
 * with i > 0 (no comma before the first, none after the last), so no
 * object ever ends with a trailing comma.  `n` is unused; callers keep
 * passing it for symmetry with the array helpers. */
static void jsep(FILE *f, int pretty, int level, int i, int n)
{
    (void)n;
    if (!pretty) {
        if (i > 0) fputs(", ", f);
        return;
    }
    if (i > 0) fputs("\n", f);          /* first field follows "{" directly */
    indent_line(f, level);
    if (i > 0) fputc(',', f);
}

static void jf_str(FILE *f, const char *k, const char *v,
                   int pretty, int level, int *i, int n)
{
    jsep(f, pretty, level, *i, n);
    fprintf(f, "\"%s\":", k);
    js(f, v);
    (*i)++;
}

static void jf_num(FILE *f, const char *k, int v,
                   int pretty, int level, int *i, int n)
{
    jsep(f, pretty, level, *i, n);
    fprintf(f, "\"%s\":%d", k, v);
    (*i)++;
}

/* [ "a", "b", ... ] — level = indent of the element lines */
static void jstrarr(FILE *f, const char *const *a, size_t n,
                    int pretty, int level)
{
    if (n == 0) { fputs("[]", f); return; }
    if (!pretty) {
        fputc('[', f);
        for (size_t k = 0; k < n; k++) {
            if (k) fputc(',', f);
            js(f, a[k]);
        }
        fputc(']', f);
        return;
    }
    fputs("[\n", f);
    for (size_t k = 0; k < n; k++) {
        indent_line(f, level);
        js(f, a[k]);
        fputs(k + 1 < n ? ",\n" : "\n", f);
    }
    indent_line(f, level - 1);
    fputc(']', f);
}

static void jf_strarr(FILE *f, const char *k, const char *const *a, size_t n,
                      int pretty, int level, int *i, int ntop)
{
    jsep(f, pretty, level, *i, ntop);
    fprintf(f, "\"%s\":", k);
    jstrarr(f, a, n, pretty, level + 1);
    (*i)++;
}

/* [ {"name":"x","has_value":true,"required":false}, ... ] */
static void jflagarr(FILE *f, const tool_flag_t *fl, size_t n,
                     int pretty, int level)
{
    if (n == 0) { fputs("[]", f); return; }
    if (!pretty) {
        fputc('[', f);
        for (size_t k = 0; k < n; k++) {
            if (k) fputc(',', f);
            fprintf(f, "{\"name\":");
            js(f, fl[k].name);
            fprintf(f, ",\"has_value\":%s,\"required\":%s}",
                    fl[k].has_value ? "true" : "false",
                    fl[k].required ? "true" : "false");
        }
        fputc(']', f);
        return;
    }
    fputs("[\n", f);
    for (size_t k = 0; k < n; k++) {
        indent_line(f, level);
        fputs("{\n", f);
        indent_line(f, level + 1);
        fprintf(f, "\"name\":");
        js(f, fl[k].name);
        fputs(",\n", f);
        indent_line(f, level + 1);
        fprintf(f, "\"has_value\":%s,\n",
                fl[k].has_value ? "true" : "false");
        indent_line(f, level + 1);
        fprintf(f, "\"required\":%s\n",
                fl[k].required ? "true" : "false");
        indent_line(f, level);
        fputs(k + 1 < n ? "},\n" : "}\n", f);
    }
    indent_line(f, level - 1);
    fputc(']', f);
}

static void jf_flagarr(FILE *f, const char *k, const tool_flag_t *fl, size_t n,
                       int pretty, int level, int *i, int ntop)
{
    jsep(f, pretty, level, *i, ntop);
    fprintf(f, "\"%s\":", k);
    jflagarr(f, fl, n, pretty, level + 1);
    (*i)++;
}

/* [ {"name":"x","required":true,"type":"positive-int"}, ... ] */
static void jposarr(FILE *f, const tool_pos_t *p, size_t n,
                    int pretty, int level)
{
    if (n == 0) { fputs("[]", f); return; }
    if (!pretty) {
        fputc('[', f);
        for (size_t k = 0; k < n; k++) {
            if (k) fputc(',', f);
            fprintf(f, "{\"name\":");
            js(f, p[k].name);
            fprintf(f, ",\"required\":%s,\"type\":",
                    p[k].required ? "true" : "false");
            js(f, p[k].type);
            fputc('}', f);
        }
        fputc(']', f);
        return;
    }
    fputs("[\n", f);
    for (size_t k = 0; k < n; k++) {
        indent_line(f, level);
        fputs("{\n", f);
        indent_line(f, level + 1);
        fprintf(f, "\"name\":");
        js(f, p[k].name);
        fputs(",\n", f);
        indent_line(f, level + 1);
        fprintf(f, "\"required\":%s,\n",
                p[k].required ? "true" : "false");
        indent_line(f, level + 1);
        fprintf(f, "\"type\":");
        js(f, p[k].type);
        fputs("\n", f);
        indent_line(f, level);
        fputs(k + 1 < n ? "},\n" : "}\n", f);
    }
    indent_line(f, level - 1);
    fputc(']', f);
}

static void jf_posarr(FILE *f, const char *k, const tool_pos_t *p, size_t n,
                      int pretty, int level, int *i, int ntop)
{
    jsep(f, pretty, level, *i, ntop);
    fprintf(f, "\"%s\":", k);
    jposarr(f, p, n, pretty, level + 1);
    (*i)++;
}

static void jf_jsonkeys(FILE *f, const tool_entry_t *e,
                        int pretty, int level, int *i, int ntop)
{
    jsep(f, pretty, level, *i, ntop);
    fputs("\"json_keys\":", f);
    if (!pretty) {
        fputs("{\"required\":", f);
        jstrarr(f, e->json_req, e->n_json_req, 0, 0);
        fputs(",\"optional\":", f);
        jstrarr(f, e->json_opt, e->n_json_opt, 0, 0);
        fputs("}", f);
    } else {
        fputs("{\n", f);
        indent_line(f, level + 1);
        fputs("\"required\":", f);
        jstrarr(f, e->json_req, e->n_json_req, 1, level + 2);
        fputs(",\n", f);
        indent_line(f, level + 1);
        fputs("\"optional\":", f);
        jstrarr(f, e->json_opt, e->n_json_opt, 1, level + 2);
        fputs("\n", f);
        indent_line(f, level);
        fputc('}', f);
    }
    (*i)++;
}

static void jf_entity_aliases(FILE *f, int pretty, int level, int *i, int n)
{
    jsep(f, pretty, level, *i, n);
    fputs("\"entity_aliases\":", f);
    if (!pretty) {
        fputs("{\"exec\":[\"execution\"],\"log\":[\"execution_log\"]}", f);
    } else {
        fputs("{\n", f);
        indent_line(f, level + 1);
        fputs("\"exec\":[\n", f);
        indent_line(f, level + 2);
        fputs("\"execution\"\n", f);
        indent_line(f, level + 1);
        fputs("],\n", f);
        indent_line(f, level + 1);
        fputs("\"log\":[\n", f);
        indent_line(f, level + 2);
        fputs("\"execution_log\"\n", f);
        indent_line(f, level + 1);
        fputs("]\n", f);
        indent_line(f, level);
        fputc('}', f);
    }
    (*i)++;
}

static void jf_exit_codes(FILE *f, int pretty, int level, int *i, int n)
{
    const exit_code_t *ec = exit_codes;
    const int m = (int)(sizeof(exit_codes) / sizeof(exit_codes[0]));

    jsep(f, pretty, level, *i, n);
    fputs("\"exit_codes\":", f);
    if (!pretty) {
        fputc('{', f);
        for (int k = 0; k < m; k++) {
            if (k) fputc(',', f);
            fprintf(f, "\"%s\":", ec[k].code);
            js(f, ec[k].meaning);
        }
        fputc('}', f);
    } else {
        fputs("{\n", f);
        for (int k = 0; k < m; k++) {
            indent_line(f, level + 1);
            fprintf(f, "\"%s\":", ec[k].code);
            js(f, ec[k].meaning);
            fputs(k + 1 < m ? ",\n" : "\n", f);
        }
        indent_line(f, level);
        fputc('}', f);
    }
    (*i)++;
}

static void jf_error(FILE *f, int pretty, int level, int *i, int n)
{
    jsep(f, pretty, level, *i, n);
    fputs("\"error\":", f);
    if (!pretty) {
        fputs("{\"stream\":\"stderr\",\"line1\":", f);
        js(f, "{\"error\":\"ACTA_DB_ERR_*\"|\"ACTA_CLI_ERR\","
              "\"code\":-<exit>,\"message\":\"...\"}");
        fputs(",\"invariant\":\"code == -exit\"}", f);
    } else {
        fputs("{\n", f);
        indent_line(f, level + 1);
        fputs("\"stream\":", f);
        js(f, "stderr");
        fputs(",\n", f);
        indent_line(f, level + 1);
        fputs("\"line1\":", f);
        js(f, "{\"error\":\"ACTA_DB_ERR_*\"|\"ACTA_CLI_ERR\","
              "\"code\":-<exit>,\"message\":\"...\"}");
        fputs(",\n", f);
        indent_line(f, level + 1);
        fputs("\"invariant\":", f);
        js(f, "code == -exit");
        fputs("\n", f);
        indent_line(f, level);
        fputc('}', f);
    }
    (*i)++;
}

static void emit_entry(FILE *f, const tool_entry_t *e, int pretty,
                       int t, int n)
{
    const int has_keys = (e->n_json_req > 0 || e->n_json_opt > 0);
    const int ef = 9 + (has_keys ? 1 : 0);
    int k = 0;

    if (!pretty) {
        if (t > 0) fputc(',', f);
        fputs("{", f);
    } else {
        fputs("    {\n", f);
    }

    jf_str(f, "command", e->command,   pretty, 3, &k, ef);
    jf_str(f, "entity",  e->entity,     pretty, 3, &k, ef);
    jf_str(f, "action",  e->action,     pretty, 3, &k, ef);
    jf_strarr(f, "aliases", e->aliases, e->n_aliases, pretty, 3, &k, ef);
    jf_str(f, "description", e->description, pretty, 3, &k, ef);
    jf_posarr(f, "positionals", e->positionals, e->n_pos, pretty, 3, &k, ef);
    jf_flagarr(f, "flags", e->flags, e->n_flags, pretty, 3, &k, ef);
    jf_str(f, "input", e->input, pretty, 3, &k, ef);
    if (has_keys)
        jf_jsonkeys(f, e, pretty, 3, &k, ef);
    jf_str(f, "success", e->success, pretty, 3, &k, ef);

    if (!pretty) {
        fputc('}', f);
    } else {
        indent_line(f, 2);
        fputs(t + 1 < n ? "},\n" : "}\n", f);
    }
}

/* ------------------------------------------------------------------ */
/*  tools_print                                                        */
/* ------------------------------------------------------------------ */

int tools_print(FILE *out, int pretty)
{
    const int top_n = 9;  /* name, version, usage, global_flags,
                            entity_aliases, input_sources, exit_codes,
                            error, tools */
    int i = 0;

    fputs(pretty ? "{\n" : "{", out);

    jf_str(out, "name", "acta_cli", pretty, 1, &i, top_n);
    jf_num(out, "version", 1, pretty, 1, &i, top_n);
    jf_str(out, "usage",
           "acta_cli [global flags] <entity> <action> [args]",
           pretty, 1, &i, top_n);
    jf_flagarr(out, "global_flags", global_flags,
               sizeof(global_flags) / sizeof(global_flags[0]),
               pretty, 1, &i, top_n);
    jf_entity_aliases(out, pretty, 1, &i, top_n);
    jf_strarr(out, "input_sources", input_sources,
              sizeof(input_sources) / sizeof(input_sources[0]),
              pretty, 1, &i, top_n);
    jf_exit_codes(out, pretty, 1, &i, top_n);
    jf_error(out, pretty, 1, &i, top_n);

    jsep(out, pretty, 1, i, top_n);
    fputs("\"tools\":", out);
    if (!pretty) {
        fputc('[', out);
        for (size_t t = 0; t < TOOL_COUNT; t++)
            emit_entry(out, &tool_table[t], 0, (int)t, (int)TOOL_COUNT);
        fputc(']', out);
    } else {
        fputs("[\n", out);
        for (size_t t = 0; t < TOOL_COUNT; t++)
            emit_entry(out, &tool_table[t], 1, (int)t, (int)TOOL_COUNT);
        fputs("  ]", out);
    }

    fputs(pretty ? "\n}\n" : "}\n", out);
    return EXIT_OK;
}
