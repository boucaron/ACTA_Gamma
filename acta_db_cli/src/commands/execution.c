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
 *  Usage: set vlog_gopts at the top of cmd_exec, then call
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
 *   actagamma_db exec --help                                          */
void exec_usage(FILE *f)
{
    fputs(
"Usage: actagamma_db exec <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new execution record\n"
"  get       Fetch an execution by id\n"
"  start     Mark an execution as started\n"
"  cancel    Cancel a running execution\n"
"  complete  Mark an execution as completed\n"
"  fail      Mark an execution as failed\n"
"  set-raw   Attach raw model output to an execution\n"
"  list      List executions\n"
"  count     Count executions\n"
"  help      Show this help\n"
"\n"
"== create ===========================================================\n"
"  Create a new execution record.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    actagamma_db exec create \\\n"
"      --prompt \"What is the capital of France?\" \\\n"
"      --context_id 1 --skill_revision_id 3 \\\n"
"      --model_revision_id 2 --status pending\n"
"        <- flag-based\n"
"\n"
"    cat exec.json | actagamma_db exec create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --prompt <str>               The prompt / task description\n"
"    --context_id <int>           Owning context (must be > 0)\n"
"    --skill_revision_id <int>    Skill revision (must be > 0)\n"
"    --model_revision_id <int>    Model revision (must be > 0)\n"
"\n"
"  Optional fields:\n"
"    --parent_execution_id <int>  Parent execution (for sub-tasks)\n"
"    --status <str>               pending | running | completed | failed | cancelled\n"
"\n"
"  Options:\n"
"    --json               Read the record as JSON from stdin\n"
"    --id-only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single execution by its primary key.\n"
"\n"
"    actagamma_db exec get 42\n"
"\n"
"  Options:\n"
"    --id-only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== start <id> ======================================================\n"
"  Transition an execution to 'running'.\n"
"\n"
"    actagamma_db exec start 42\n"
"\n"
"== cancel <id> ====================================================\n"
"  Transition an execution to 'cancelled'.\n"
"\n"
"    actagamma_db exec cancel 42\n"
"\n"
"== complete <id> ==================================================\n"
"  Transition an execution to 'completed'.\n"
"\n"
"    actagamma_db exec complete 42 --result \"answer text\"\n"
"\n"
"  Options:\n"
"    --result <str>       Final result / answer text\n"
"\n"
"== fail <id> ======================================================\n"
"  Transition an execution to 'failed'.\n"
"\n"
"    actagamma_db exec fail 42 --error \"timeout after 30s\"\n"
"\n"
"  Options:\n"
"    --error <str>        Error description\n"
"\n"
"== set-raw <id> ====================================================\n"
"  Attach the raw model response to an execution.\n"
"\n"
"    actagamma_db exec set-raw 42 --raw '<full raw output>'\n"
"\n"
"  Options:\n"
"    --raw <str>          Raw response text (required)\n"
"\n"
"== list ============================================================\n"
"  List executions with optional filters.\n"
"\n"
"    actagamma_db exec list\n"
"    actagamma_db exec list --status running --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context-id <int>         Filter by context\n"
"    --skill-revision-id <int>  Filter by skill revision\n"
"    --model-revision-id <int>  Filter by model revision\n"
"    --parent-execution-id <int> Filter by parent execution\n"
"    --offset <n>               Skip first N rows (default 0)\n"
"    --limit <n>                Max rows to return (default 0 = unlimited)\n"
"    --count                    Return only the row count (no rows)\n"
"    --table                    Columnar output instead of JSON\n"
"    --fields <csv>             Comma-separated field filter\n"
"    --no_nulls                 Omit null-valued fields from JSON\n"
"\n"
"== count ===========================================================\n"
"  Count executions matching optional filters.\n"
"\n"
"    actagamma_db exec count\n"
"    actagamma_db exec count --status failed --context-id 7\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context-id <int>         Filter by context\n"
"    --skill-revision-id <int>  Filter by skill revision\n"
"    --model-revision-id <int>  Filter by model revision\n"
"    --parent-execution-id <int> Filter by parent execution\n"
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
"  Create a new execution record.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    actagamma_db exec create \\\n"
"      --prompt \"What is the capital of France?\" \\\n"
"      --context_id 1 --skill_revision_id 3 \\\n"
"      --model_revision_id 2 --status pending\n"
"        <- flag-based\n"
"\n"
"    cat exec.json | actagamma_db exec create --json\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --prompt <str>               The prompt / task description\n"
"    --context_id <int>           Owning context (must be > 0)\n"
"    --skill_revision_id <int>    Skill revision (must be > 0)\n"
"    --model_revision_id <int>    Model revision (must be > 0)\n"
"\n"
"  Optional fields:\n"
"    --parent_execution_id <int>  Parent execution (for sub-tasks)\n"
"    --status <str>               pending | running | completed | failed | cancelled\n"
"\n"
"  Options:\n"
"    --json               Read the record as JSON from stdin\n"
"    --id-only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single execution by its primary key.\n"
"\n"
"    actagamma_db exec get 42\n"
"\n"
"  Options:\n"
"    --id-only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_start(FILE *f)
{
    fputs(
"== start <id> ======================================================\n"
"  Transition an execution to 'running'.\n"
"\n"
"    actagamma_db exec start 42\n", f);
}

static void usage_cancel(FILE *f)
{
    fputs(
"== cancel <id> ====================================================\n"
"  Transition an execution to 'cancelled'.\n"
"\n"
"    actagamma_db exec cancel 42\n", f);
}

static void usage_complete(FILE *f)
{
    fputs(
"== complete <id> ==================================================\n"
"  Transition an execution to 'completed'.\n"
"\n"
"    actagamma_db exec complete 42 --result \"answer text\"\n"
"\n"
"  Options:\n"
"    --result <str>       Final result / answer text\n", f);
}

static void usage_fail(FILE *f)
{
    fputs(
"== fail <id> ======================================================\n"
"  Transition an execution to 'failed'.\n"
"\n"
"    actagamma_db exec fail 42 --error \"timeout after 30s\"\n"
"\n"
"  Options:\n"
"    --error <str>        Error description\n", f);
}

static void usage_set_raw(FILE *f)
{
    fputs(
"== set-raw <id> ====================================================\n"
"  Attach the raw model response to an execution.\n"
"\n"
"    actagamma_db exec set-raw 42 --raw '<full raw output>'\n"
"\n"
"  Options:\n"
"    --raw <str>          Raw response text (required)\n", f);
}

static void usage_list(FILE *f)
{
    fputs(
"== list ============================================================\n"
"  List executions with optional filters.\n"
"\n"
"    actagamma_db exec list\n"
"    actagamma_db exec list --status running --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context-id <int>         Filter by context\n"
"    --skill-revision-id <int>  Filter by skill revision\n"
"    --model-revision-id <int>  Filter by model revision\n"
"    --parent-execution-id <int> Filter by parent execution\n"
"    --offset <n>               Skip first N rows (default 0)\n"
"    --limit <n>                Max rows to return (default 0 = unlimited)\n"
"    --count                    Return only the row count (no rows)\n"
"    --table                    Columnar output instead of JSON\n"
"    --fields <csv>             Comma-separated field filter\n"
"    --no_nulls                 Omit null-valued fields from JSON\n", f);
}

static void usage_count(FILE *f)
{
    fputs(
"== count ===========================================================\n"
"  Count executions matching optional filters.\n"
"\n"
"    actagamma_db exec count\n"
"    actagamma_db exec count --status failed --context-id 7\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context-id <int>         Filter by context\n"
"    --skill-revision-id <int>  Filter by skill revision\n"
"    --model-revision-id <int>  Filter by model revision\n"
"    --parent-execution-id <int> Filter by parent execution\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_exec_fields(const char *tag, const execution_t *e)
{
    VLOG(2, "%s: id=%d ctx=%d skill_rev=%d model_rev=%d status=%s prompt=%s "
         "result=%s error=%s started_at=%s completed_at=%s parent=%d",
         tag,
         e->id,
         e->context_id,
         e->skill_revision_id,
         e->model_revision_id,
         e->status           ? e->status           : "(null)",
         e->prompt           ? e->prompt           : "(null)",
         e->result           ? e->result           : "(null)",
         e->error            ? e->error            : "(null)",
         e->started_at       ? e->started_at       : "(null)",
         e->completed_at     ? e->completed_at     : "(null)",
         e->parent_execution_id);
}

static void vlog_exec_raw(const char *tag, const execution_t *e, int rc)
{
    VLOG(3, "%s: exec=%p id=%d rc=%d",
         tag, (const void *)e, e ? e->id : -1, rc);
}

/* ── execution_t → JSON object ────────────────────────────────────── */

static void exec_to_json(FILE *f, const execution_t *e, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", e->id);
    }
    if ((!fl || fields_has(fl, "context_id")) && !(gopts->no_nulls && e->context_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"context_id\":%d", e->context_id);
    }
    if ((!fl || fields_has(fl, "skill_revision_id")) && !(gopts->no_nulls && e->skill_revision_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"skill_revision_id\":%d", e->skill_revision_id);
    }
    if ((!fl || fields_has(fl, "model_revision_id")) && !(gopts->no_nulls && e->model_revision_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"model_revision_id\":%d", e->model_revision_id);
    }
    if ((!fl || fields_has(fl, "prompt")) && !(gopts->no_nulls && !e->prompt)) {
        if (shown++) fputs(", ", f);
        fputs("\"prompt\":", f);
        if (e->prompt) json_str(f, e->prompt); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "raw_response")) && !(gopts->no_nulls && !e->raw_response)) {
        if (shown++) fputs(", ", f);
        fputs("\"raw_response\":", f);
        if (e->raw_response) json_str(f, e->raw_response); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "result")) && !(gopts->no_nulls && !e->result)) {
        if (shown++) fputs(", ", f);
        fputs("\"result\":", f);
        if (e->result) json_str(f, e->result); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "status")) && !(gopts->no_nulls && !e->status)) {
        if (shown++) fputs(", ", f);
        fputs("\"status\":", f);
        if (e->status) json_str(f, e->status); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "error")) && !(gopts->no_nulls && !e->error)) {
        if (shown++) fputs(", ", f);
        fputs("\"error\":", f);
        if (e->error) json_str(f, e->error); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !e->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (e->created_at) json_str(f, e->created_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "started_at")) && !(gopts->no_nulls && !e->started_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"started_at\":", f);
        if (e->started_at) json_str(f, e->started_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "completed_at")) && !(gopts->no_nulls && !e->completed_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"completed_at\":", f);
        if (e->completed_at) json_str(f, e->completed_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "parent_execution_id")) && !(gopts->no_nulls && e->parent_execution_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"parent_execution_id\":%d", e->parent_execution_id);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void exec_table(FILE *f, const execution_t *e, int header)
{
    if (header) {
        fprintf(f, " %4s  %4s  %-10s  %-38s  %-22s  %-22s\n",
                "ID", "CTX", "STATUS", "PROMPT", "STARTED_AT", "COMPLETED_AT");
        return;
    }
    char idb[16];
    char ctxb[16];
    snprintf(idb, sizeof idb, "%d", e->id);
    snprintf(ctxb, sizeof ctxb, "%d", e->context_id);
    fprintf(f, " %4s  ", idb);
    fprintf(f, " %4s  ", ctxb);
    tcol(f, e->status,       10);
    tcol(f, e->prompt,      38);
    tcol(f, e->started_at,  22);
    tcol(f, e->completed_at, 22);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t exec_actions[] = {
    { "create",   "create a new exec record"      },
    { "get",      "fetch an exec by id"           },
    { "start",    "mark exec as started"          },
    { "cancel",   "cancel a running exec"         },
    { "complete", "mark exec as completed"        },
    { "fail",     "mark exec as failed"           },
    { "set-raw",  "attach raw output to an exec"  },
    { "list",     "list execs"                    },
    { "count",    "count execs"                   },
    { "help",     "show this help"                },
};
#define EXEC_ACTIONS (sizeof(exec_actions) / sizeof(exec_actions[0]))

/* ── valid_status ─────────────────────────────────────────────────── */
static int valid_status(const char *s)
{
    if (!s)
        return 0;
    return strcmp(s, "pending")   == 0 ||
           strcmp(s, "running")   == 0 ||
           strcmp(s, "completed") == 0 ||
           strcmp(s, "failed")    == 0 ||
           strcmp(s, "cancelled") == 0;
}


int cmd_exec(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
             db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── help (subcommand-level; only the bare word "help") ──────── */
    if (strcmp(action, "help") == 0) {
        exec_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        execution_t exec = {0};
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
            VLOG(1, "exec create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_execution(blob, &exec) != 0) {
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
            const char *f_prompt          = cmd_args_flag(ga, "prompt", 1);
            const char *f_ctx_id          = cmd_args_flag(ga, "context_id", 1);
            const char *f_skill_id        = cmd_args_flag(ga, "skill_revision_id", 1);
            const char *f_model_id        = cmd_args_flag(ga, "model_revision_id", 1);
            const char *f_parent_id       = cmd_args_flag(ga, "parent_execution_id", 1);
            const char *f_status          = cmd_args_flag(ga, "status", 1);

            exec.prompt             = (char *)f_prompt;
            exec.status             = (char *)f_status;
            exec.context_id         = f_ctx_id    ? atoi(f_ctx_id)    : 0;
            exec.skill_revision_id  = f_skill_id  ? atoi(f_skill_id)  : 0;
            exec.model_revision_id  = f_model_id  ? atoi(f_model_id)  : 0;
            exec.parent_execution_id = f_parent_id ? atoi(f_parent_id) : 0;
        }

        VLOG(1, "exec create: prompt=%s context_id=%d skill_revision_id=%d "
                "model_revision_id=%d parent_execution_id=%d status=%s",
             exec.prompt ? exec.prompt : "(missing)",
             exec.context_id, exec.skill_revision_id,
             exec.model_revision_id, exec.parent_execution_id,
             exec.status ? exec.status : "(default)");

        VLOG(2, "  params: prompt=%s context_id=%d skill_revision_id=%d "
                "model_revision_id=%d parent_execution_id=%d status=%s "
                "fields=%s no_nulls=%d id_only=%d table=%d",
             exec.prompt ? exec.prompt : "(null)",
             exec.context_id, exec.skill_revision_id,
             exec.model_revision_id, exec.parent_execution_id,
             exec.status ? exec.status : "(null)",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p json_owned=%d exec=%p prompt=%p status=%p",
             (const void *)ga, json_owned, (const void *)&exec,
             (const void *)exec.prompt, (const void *)exec.status);

        /* ── required-field validation ────────────────────────────── */
        if (!exec.prompt) {
            VLOG(1, "  ERROR: missing required field 'prompt'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: prompt\"}\n");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }
        if (exec.context_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'context_id'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: context_id\"}\n");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }
        if (exec.skill_revision_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'skill_revision_id'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: skill_revision_id\"}\n");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }
        if (exec.model_revision_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'model_revision_id'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: model_revision_id\"}\n");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }

        /* ── optional-field validation ────────────────────────────── */
        if (exec.status && !valid_status(exec.status)) {
            VLOG(1, "  ERROR: 'status' must be one of: pending, running, "
                    "completed, failed, cancelled (got '%s')",
                    exec.status);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"status must be one of: pending, running, "
                "completed, failed, cancelled\"}\n");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }

        vlog_exec_fields("  pre-create", &exec);
        VLOG(3, "  exec=%p &out_id=%p",
             (const void *)&exec, (const void *)&exec.id);

        int out_id = 0;
        int rc = acta_db_execution_create(db, &exec, &out_id);

        VLOG(3, "  acta_db_execution_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = map_rc_to_exit(rc);
            goto cleanup_exec_create;
        }

        VLOG(1, "  created exec id=%d", out_id);

        if (gopts->id_only)
            fprintf(stdout, "%d\n", out_id);
        else
            fprintf(stdout, "{\"id\":%d}\n", out_id);

        ret = EXIT_OK;
        goto cleanup_exec_create;

    cleanup_exec_create:
        if (json_owned) {
            free(exec.prompt);
            free((void *)exec.status);
            free(exec.created_at);
        }
        return ret;
    }


    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "exec get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_get(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "exec get: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_get(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "exec get: fetching id=%d", id);

        int err = 0;
        execution_t *e = acta_db_execution_get(db, id, &err);

        VLOG(3, "  acta_db_execution_get(%d) → ptr=%p err=%d",
             id, (const void *)e, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_execution_free(e);
            return map_rc_to_exit(err);
        }
        if (!e) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

        vlog_exec_fields("  result", e);
        vlog_exec_raw("  raw", e, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", e->id);
        } else if (gopts->table) {
            exec_table(stdout, NULL, 1);
            exec_table(stdout, e, 0);
        } else {
            exec_to_json(stdout, e, gopts);
            fputc('\n', stdout);
        }
        acta_db_execution_free(e);
        return EXIT_OK;
    }

    /* ── start <id> ───────────────────────────────────────────────── */
    if (strcmp(action, "start") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "exec start: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_start(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "exec start: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_start(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "exec start: id=%d", id);

        int rc = acta_db_execution_start(db, id);
        VLOG(3, "  acta_db_execution_start(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return map_rc_to_exit(rc);
        }
        VLOG(1, "  started id=%d", id);
        return EXIT_OK;
    }

    /* ── cancel <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "cancel") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "exec cancel: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_cancel(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "exec cancel: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_cancel(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "exec cancel: id=%d", id);

        int rc = acta_db_execution_cancel(db, id);
        VLOG(3, "  acta_db_execution_cancel(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return map_rc_to_exit(rc);
        }
        VLOG(1, "  cancelled id=%d", id);
        return EXIT_OK;
    }

    /* ── complete <id> ────────────────────────────────────────────── */
    if (strcmp(action, "complete") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "exec complete: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_complete(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "exec complete: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_complete(stderr);
            return EXIT_INVALID;
        }

        const char *f_result = cmd_args_flag(ga, "result", 0);

        VLOG(1, "exec complete: id=%d result=%s",
             id, f_result ? f_result : "(null)");

        int rc = acta_db_execution_complete(db, id, f_result);
        VLOG(3, "  acta_db_execution_complete(%d, %p) → rc=%d",
             id, (const void *)f_result, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return map_rc_to_exit(rc);
        }
        VLOG(1, "  completed id=%d", id);
        return EXIT_OK;
    }

    /* ── fail <id> ────────────────────────────────────────────────── */
    if (strcmp(action, "fail") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "exec fail: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_fail(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "exec fail: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_fail(stderr);
            return EXIT_INVALID;
        }

        const char *f_error = cmd_args_flag(ga, "error", 0);

        VLOG(1, "exec fail: id=%d error=%s",
             id, f_error ? f_error : "(null)");

        int rc = acta_db_execution_fail(db, id, f_error);
        VLOG(3, "  acta_db_execution_fail(%d, %p) → rc=%d",
             id, (const void *)f_error, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return map_rc_to_exit(rc);
        }
        VLOG(1, "  failed id=%d", id);
        return EXIT_OK;
    }

    /* ── set-raw <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "set-raw") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "exec set-raw: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            usage_set_raw(stderr);
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "exec set-raw: invalid id=%s", id_str);
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"invalid <id>: must be a positive integer\"}\n");
            usage_set_raw(stderr);
            return EXIT_INVALID;
        }

        const char *f_raw = cmd_args_flag(ga, "raw", 1);
        if (!f_raw) {
            VLOG(1, "exec set-raw: ERROR missing required --raw");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required flag: --raw\"}\n");
            usage_set_raw(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "exec set-raw: id=%d", id);

        int rc = acta_db_execution_set_raw_response(db, id, f_raw);
        VLOG(3, "  acta_db_execution_set_raw_response(%d, %p) → rc=%d",
             id, (const void *)f_raw, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return map_rc_to_exit(rc);
        }
        VLOG(1, "  set raw response for id=%d", id);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *f_status   = cmd_args_flag(ga, "status", 1);
        const char *f_ctx_id   = cmd_args_flag(ga, "context-id", 1);
        const char *f_skill_id = cmd_args_flag(ga, "skill-revision-id", 1);
        const char *f_model_id = cmd_args_flag(ga, "model-revision-id", 1);
        const char *f_parent   = cmd_args_flag(ga, "parent-execution-id", 1);
        const char *s_off      = cmd_args_flag(ga, "offset", 1);
        const char *s_lim      = cmd_args_flag(ga, "limit", 1);

        int offset = 0, limit = 0;

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
            limit = (int)v;   /* 0 = no limit (documented) */
        }

        execution_query_t q = {
            .status              = f_status,
            .context_id          = f_ctx_id   ? atoi(f_ctx_id)   : 0,
            .skill_revision_id   = f_skill_id ? atoi(f_skill_id) : 0,
            .model_revision_id   = f_model_id ? atoi(f_model_id) : 0,
            .parent_execution_id = f_parent   ? atoi(f_parent)   : 0,
        };

        VLOG(1, "exec list: status=%s ctx=%d skill_rev=%d model_rev=%d "
                "parent=%d offset=%d limit=%d",
             f_status ? f_status : "(any)",
             q.context_id, q.skill_revision_id, q.model_revision_id,
             q.parent_execution_id, offset, limit);

        VLOG(2, "  full: status=%s ctx=%s skill_rev=%s model_rev=%s parent=%s "
                "offset=%d limit=%d no_nulls=%d table=%d fields=%s",
             f_status   ? f_status   : "(null)",
             f_ctx_id   ? f_ctx_id   : "(null)",
             f_skill_id ? f_skill_id : "(null)",
             f_model_id ? f_model_id : "(null)",
             f_parent   ? f_parent   : "(null)",
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  q=%p status=%p ctx=%d skill_rev=%d model_rev=%d parent=%d",
             (const void *)&q, (const void *)q.status,
             q.context_id, q.skill_revision_id,
             q.model_revision_id, q.parent_execution_id);

        if (gopts->count) {
            int err = 0;
            int n = acta_db_execution_count(db, &q, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        execution_t **items = acta_db_execution_query(db, &q, offset, limit,
                                                      &out_count, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  query FAILED err=%d", err);
            acta_db_execution_list_free(items, out_count);
            return map_rc_to_exit(err);
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_exec_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            exec_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                exec_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                exec_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_execution_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count ────────────────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *f_status   = cmd_args_flag(ga, "status", 1);
        const char *f_ctx_id   = cmd_args_flag(ga, "context-id", 1);
        const char *f_skill_id = cmd_args_flag(ga, "skill-revision-id", 1);
        const char *f_model_id = cmd_args_flag(ga, "model-revision-id", 1);
        const char *f_parent   = cmd_args_flag(ga, "parent-execution-id", 1);

        execution_query_t q = {
            .status              = f_status,
            .context_id          = f_ctx_id   ? atoi(f_ctx_id)   : 0,
            .skill_revision_id   = f_skill_id ? atoi(f_skill_id) : 0,
            .model_revision_id   = f_model_id ? atoi(f_model_id) : 0,
            .parent_execution_id = f_parent   ? atoi(f_parent)   : 0,
        };

        VLOG(1, "exec count: status=%s ctx=%d skill_rev=%d model_rev=%d parent=%d",
             f_status ? f_status : "(any)",
             q.context_id, q.skill_revision_id,
             q.model_revision_id, q.parent_execution_id);

        VLOG(2, "  q=%p status=%p ctx=%d skill_rev=%d model_rev=%d parent=%d",
             (const void *)&q, (const void *)q.status,
             q.context_id, q.skill_revision_id,
             q.model_revision_id, q.parent_execution_id);

        int err = 0;
        int n = acta_db_execution_count(db, &q, &err);
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
        const char *guess = closest_action(action, exec_actions, EXEC_ACTIONS);

        VLOG(1, "exec: unknown action '%s'%s",
             action ? action : "(null)",
             guess   ? "  (suggestion below)" : "");

        fprintf(stderr, "Unknown action '%s'.\n", action ? action : "(null)");
        if (guess)
            fprintf(stderr, "  Did you mean '%s'?\n", guess);
        fprintf(stderr, "  Run 'actagamma_db exec help' for full usage.\n");
        return EXIT_INVALID;
    }
}
