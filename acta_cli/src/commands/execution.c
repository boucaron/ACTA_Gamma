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
 *   acta_cli exec --help                                          */
static void exec_usage(FILE *f)
{
    fputs(
"Usage: acta_cli exec <action> [options]\n"
"\n"
"Actions:\n"
"  create    Create a new execution record\n"
"  get       Fetch an execution by id\n"
"  delete    Remove an execution (soft delete)\n"
"  restore   Restore a deleted execution\n"
"  start     Mark an execution as started\n"
"  cancel    Cancel a running execution\n"
"  complete  Mark an execution as completed\n"
"  fail      Mark an execution as failed\n"
"  reset     Reset a failed execution back to pending\n"
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
"    acta_cli exec create \\\n"
"      --prompt \"What is the capital of France?\" \\\n"
"      --context_id 1 --skill_revision_id 3 \\\n"
"      --model_revision_id 2\n"
"        <- flag-based\n"
"\n"
"    cat exec.json | acta_cli exec create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --context_id <int>           Owning context (must be > 0)\n"
"    --skill_revision_id <int>    Skill revision (must be > 0)\n"
"    --model_revision_id <int>    Model revision (must be > 0)\n"
"\n"
"  Optional fields:\n"
"    --prompt <str>               Prompt / task description\n"
"    --parent_execution_id <int>  Parent execution (for sub-tasks)\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the record as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single execution by its primary key.\n"
"\n"
"    acta_cli exec get 42\n"
"    acta_cli exec get 42 --include_deleted\n"
"\n"
"  Options:\n"
"    --include_deleted    Return the row even if soft-deleted\n"
"    --deleted            Alias for --include_deleted\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== delete <id> ====================================================\n"
"  Soft-delete an execution (sets deleted_at).\n"
"  Refused from the 'running' state and on already-deleted rows.\n"
"\n"
"    acta_cli exec delete 42\n"
"\n"
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted execution (status is\n"
"  untouched; a deleted 'failed' row restores to 'failed').\n"
"\n"
"    acta_cli exec restore 42\n"
"\n"
"== start <id> ======================================================\n"
"  Transition an execution to 'running'.\n"
"\n"
"    acta_cli exec start 42\n"
"\n"
"== cancel <id> ====================================================\n"
"  Transition an execution to 'cancelled'.\n"
"\n"
"    acta_cli exec cancel 42\n"
"\n"
"== complete <id> ==================================================\n"
"  Transition an execution to 'completed'.\n"
"\n"
"    acta_cli exec complete 42 --result \"answer text\"\n"
"\n"
"  Options:\n"
"    --result <str>       Final result / answer text\n"
"\n"
"== fail <id> ======================================================\n"
"  Transition an execution to 'failed'.\n"
"\n"
"    acta_cli exec fail 42 --error \"timeout after 30s\"\n"
"\n"
"  Options:\n"
"    --error <str>        Error description\n"
"\n"
"== reset <id> ===================================================\n"
"  Transition a 'failed' execution back to 'pending' (retry).\n"
"  Clears error, raw response, started_at and completed_at;\n"
"  the execution_log audit trail of the previous attempt is kept.\n"
"\n"
"    acta_cli exec reset 42\n"
"\n"
"== set-raw <id> ====================================================\n"
"  Attach the raw model response to an execution.\n"
"\n"
"    acta_cli exec set-raw 42 --raw '<full raw output>'\n"
"\n"
"  Options:\n"
"    --raw <str>          Raw response text (required)\n"
"\n"
"== list ============================================================\n"
"  List executions with optional filters.\n"
"\n"
"    acta_cli exec list\n"
"    acta_cli exec list --status running --offset 10 --limit 25\n"
"    acta_cli exec list --include_deleted\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context_id <int>         Filter by context\n"
"    --skill_revision_id <int>  Filter by skill revision\n"
"    --model_revision_id <int>  Filter by model revision\n"
"    --parent_execution_id <int> Filter by parent execution\n"
"    --include_deleted          Include soft-deleted rows\n"
"    --deleted                  Alias for --include_deleted\n"
"    --offset <n>               Skip first N rows (default 0)\n"
"    --limit <n>                Max rows to return (default 0 = unlimited)\n"
"    --count                    Return only the row count (no rows)\n"
"    --full                     Include blob columns\n"
"                               (prompt, raw_response, result, error)\n"
"                               (default: omitted)\n"
"    --table                    Columnar output instead of JSON\n"
"    --fields <csv>             Comma-separated field filter\n"
"    --no_nulls                 Omit null-valued fields from JSON\n"
"\n"
"== count ===========================================================\n"
"  Count executions matching optional filters.\n"
"\n"
"    acta_cli exec count\n"
"    acta_cli exec count --status failed --context_id 7\n"
"    acta_cli exec count --include_deleted\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context_id <int>         Filter by context\n"
"    --skill_revision_id <int>  Filter by skill revision\n"
"    --model_revision_id <int>  Filter by model revision\n"
"    --parent_execution_id <int> Filter by parent execution\n"
"    --include_deleted          Include soft-deleted rows\n"
"    --deleted                  Alias for --include_deleted\n"
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
"  Create a new execution record.\n"
"\n"
"  Provide data via one of:\n"
"\n"
"    acta_cli exec create \\\n"
"      --prompt \"What is the capital of France?\" \\\n"
"      --context_id 1 --skill_revision_id 3 \\\n"
"      --model_revision_id 2\n"
"        <- flag-based\n"
"\n"
"    cat exec.json | acta_cli exec create --stdin\n"
"        <- JSON via stdin\n"
"\n"
"  Required fields:\n"
"    --context_id <int>           Owning context (must be > 0)\n"
"    --skill_revision_id <int>    Skill revision (must be > 0)\n"
"    --model_revision_id <int>    Model revision (must be > 0)\n"
"\n"
"  Optional fields:\n"
"    --prompt <str>               Prompt / task description\n"
"    --parent_execution_id <int>  Parent execution (for sub-tasks)\n"
"\n"
"  Options:\n"
"    --json <blob>          Read the record as JSON; --stdin and --from_file <path> are the alternative sources\n"
"    --id_only            Print only the new id (no JSON wrapper)\n"
"    --verbose <n>        debug level 0-3 (stderr)\n", f);
}

static void usage_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single execution by its primary key.\n"
"\n"
"    acta_cli exec get 42\n"
"    acta_cli exec get 42 --include_deleted\n"
"\n"
"  Options:\n"
"    --include_deleted    Return the row even if soft-deleted\n"
"    --deleted            Alias for --include_deleted\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_delete(FILE *f)
{
    fputs(
"== delete <id> ====================================================\n"
"  Soft-delete an execution (sets deleted_at).\n"
"  Refused from the 'running' state and on already-deleted rows.\n"
"\n"
"    acta_cli exec delete 42\n", f);
}

static void usage_restore(FILE *f)
{
    fputs(
"== restore <id> ===================================================\n"
"  Restore a previously soft-deleted execution (status is\n"
"  untouched; a deleted 'failed' row restores to 'failed').\n"
"\n"
"    acta_cli exec restore 42\n", f);
}

static void usage_start(FILE *f)
{
    fputs(
"== start <id> ======================================================\n"
"  Transition an execution to 'running'.\n"
"\n"
"    acta_cli exec start 42\n"
"\n"
"  stdout on success: {\"id\":N,\"status\":\"running\"} (bare N with --id_only)\n", f);
}

static void usage_cancel(FILE *f)
{
    fputs(
"== cancel <id> ====================================================\n"
"  Transition an execution to 'cancelled'.\n"
"\n"
"    acta_cli exec cancel 42\n"
"\n"
"  stdout on success: {\"id\":N,\"status\":\"cancelled\"} (bare N with --id_only)\n", f);
}

static void usage_complete(FILE *f)
{
    fputs(
"== complete <id> ==================================================\n"
"  Transition an execution to 'completed'.\n"
"\n"
"    acta_cli exec complete 42 --result \"answer text\"\n"
"\n"
"  Options:\n"
"    --result <str>       Final result / answer text\n"
"\n"
"  stdout on success: {\"id\":N,\"status\":\"completed\"} (bare N with --id_only)\n", f);
}

static void usage_fail(FILE *f)
{
    fputs(
"== fail <id> ======================================================\n"
"  Transition an execution to 'failed'.\n"
"\n"
"    acta_cli exec fail 42 --error \"timeout after 30s\"\n"
"\n"
"  Options:\n"
"    --error <str>        Error description\n"
"\n"
"  stdout on success: {\"id\":N,\"status\":\"failed\"} (bare N with --id_only)\n", f);
}

static void usage_reset(FILE *f)
{
    fputs(
"== reset <id> ===================================================\n"
"  Transition a 'failed' execution back to 'pending' (retry).\n"
"  Clears error, raw response, started_at and completed_at;\n"
"  the execution_log audit trail of the previous attempt is kept.\n"
"\n"
"    acta_cli exec reset 42\n"
"\n"
"  stdout on success: {\"id\":N,\"status\":\"pending\"} (bare N with --id_only)\n", f);
}

static void usage_set_raw(FILE *f)
{
    fputs(
"== set-raw <id> ====================================================\n"
"  Attach the raw model response to an execution.\n"
"\n"
"    acta_cli exec set-raw 42 --raw '<full raw output>'\n"
"\n"
"  Options:\n"
"    --raw <str>          Raw response text (required)\n"
"\n"
"  stdout on success: {\"id\":N,\"status\":\"<current status, unchanged>\"}\n"
"  (bare N with --id_only)\n", f);
}

static void usage_list(FILE *f)
{
    fputs(
"== list ============================================================\n"
"  List executions with optional filters.\n"
"\n"
"    acta_cli exec list\n"
"    acta_cli exec list --status running --offset 10 --limit 25\n"
"    acta_cli exec list --include_deleted\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context_id <int>         Filter by context\n"
"    --skill_revision_id <int>  Filter by skill revision\n"
"    --model_revision_id <int>  Filter by model revision\n"
"    --parent_execution_id <int> Filter by parent execution\n"
"    --include_deleted          Include soft-deleted rows\n"
"    --deleted                  Alias for --include_deleted\n"
"    --offset <n>               Skip first N rows (default 0)\n"
"    --limit <n>                Max rows to return (default 0 = unlimited)\n"
"    --count                    Return only the row count (no rows)\n"
"    --full                     Include blob columns\n"
"                               (prompt, raw_response, result, error)\n"
"                               (default: omitted)\n"
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
"    acta_cli exec count\n"
"    acta_cli exec count --status failed --context_id 7\n"
"    acta_cli exec count --include_deleted\n"
"\n"
"  Options:\n"
"    --status <str>             Filter by status\n"
"    --context_id <int>         Filter by context\n"
"    --skill_revision_id <int>  Filter by skill revision\n"
"    --model_revision_id <int>  Filter by model revision\n"
"    --parent_execution_id <int> Filter by parent execution\n"
"    --include_deleted          Include soft-deleted rows\n"
"    --deleted                  Alias for --include_deleted\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_exec_fields(const char *tag, const execution_t *e)
{
    VLOG(2, "%s: id=%d ctx=%d skill_rev=%d model_rev=%d status=%s prompt=%s "
         "result=%s error=%s started_at=%s completed_at=%s parent=%d "
         "deleted_at=%s",
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
         e->parent_execution_id,
         e->deleted_at       ? e->deleted_at       : "(null)");
}

static void vlog_exec_raw(const char *tag, const execution_t *e, int rc)
{
    VLOG(3, "%s: exec=%p id=%d rc=%d",
         tag, (const void *)e, e ? e->id : -1, rc);
}

/* free_row adapter for load_row_or_notfound (void* signature). */
static void execution_free_wrap(void *e)
{
    acta_db_execution_free((execution_t *)e);
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
    if ((!fl || fields_has(fl, "deleted_at")) && !(gopts->no_nulls && !e->deleted_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"deleted_at\":", f);
        if (e->deleted_at) json_str(f, e->deleted_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void exec_table(FILE *f, const execution_t *e, int header)
{
    if (header) {
        fprintf(f, " %4s  %4s  %-10s  %-38s  %-22s  %-22s  %-19s\n",
                "ID", "CTX", "STATUS", "PROMPT", "STARTED_AT", "COMPLETED_AT",
                "DELETED_AT");
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
    tcol(f, e->deleted_at,   19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t exec_actions[] = {
    { "create",   "create a new exec record"      },
    { "get",      "fetch an exec by id"           },
    { "delete",   "soft-delete an exec"           },
    { "restore",  "restore a deleted exec"        },
    { "start",    "mark exec as started"          },
    { "cancel",   "cancel a running exec"         },
    { "complete", "mark exec as completed"        },
    { "fail",     "mark exec as failed"           },
    { "reset",    "reset a failed exec to pending" },
    { "set-raw",  "attach raw output to an exec"  },
    { "list",     "list execs"                    },
    { "count",    "count execs"                   },
    { "help",     "show this help"                },
};
#define EXEC_ACTIONS (sizeof(exec_actions) / sizeof(exec_actions[0]))

int cmd_exec(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
             db_t *db)
{
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

        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            usage_create(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "exec create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_execution(blob, &exec) != 0) {
                VLOG(1, "  JSON parse error");
                emit_error("invalid JSON body");
                usage_create(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            exec.prompt = (char *)cmd_args_flag(ga, "prompt", 1);
            exec.status = (char *)cmd_args_flag(ga, "status", 1);

            /* Optional ref filters: absent → 0 ("all" / unset, exec is
             * zero-initialized above). */
            exec.context_id          = 0;
            exec.skill_revision_id   = 0;
            exec.model_revision_id   = 0;
            exec.parent_execution_id = 0;
            if (parse_nonneg_int_flag(ga, "context_id", &exec.context_id, 0,
                                      usage_create, "exec create") < 0 ||
                parse_nonneg_int_flag(ga, "skill_revision_id", &exec.skill_revision_id, 0,
                                      usage_create, "exec create") < 0 ||
                parse_nonneg_int_flag(ga, "model_revision_id", &exec.model_revision_id, 0,
                                      usage_create, "exec create") < 0 ||
                parse_nonneg_int_flag(ga, "parent_execution_id", &exec.parent_execution_id, 0,
                                      usage_create, "exec create") < 0)
                return EXIT_INVALID;
        }

        VLOG(1, "exec create: prompt=%s context_id=%d skill_revision_id=%d "
                "model_revision_id=%d parent_execution_id=%d status=%s",
             exec.prompt ? exec.prompt : "(none)",
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

        /* ── required-field validation ──────────────────────────────
         * prompt is optional (context-only is a valid execution; the
         * runner fails at run time if prompt and context are both
         * empty). */
        if (exec.context_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'context_id'");
            emit_error("missing required field: context_id");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }
        if (exec.skill_revision_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'skill_revision_id'");
            emit_error("missing required field: skill_revision_id");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }
        if (exec.model_revision_id <= 0) {
            VLOG(1, "  ERROR: missing required field 'model_revision_id'");
            emit_error("missing required field: model_revision_id");
            usage_create(stderr);
            ret = EXIT_INVALID;
            goto cleanup_exec_create;
        }

        /* 'status' (flag or JSON body) is rejected outright: the lib
         * always creates executions as "pending" (acta_db_execution_create
         * ignores e->status), so accepting it would only pretend to work.
         * Later state is reached via start/complete/fail/cancel. */
        if (exec.status) {
            VLOG(1, "  ERROR: 'status' is not accepted by exec create "
                    "(new executions are always 'pending')");
            emit_error("'status' is not accepted by exec create: "
                      "new executions are always 'pending'");
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
            ret = finish_op_error(db, rc, "execution create");
            goto cleanup_exec_create;
        }

        VLOG(1, "  created exec id=%d", out_id);
        emit_ok_id(gopts, out_id);

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
        int id;
        if (!parse_id_positional(ga, "id", usage_get, "exec get", &id))
            return EXIT_INVALID;

        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        VLOG(1, "exec get: fetching id=%d include_deleted=%d", id,
             include_deleted);

        int err = 0;
        /* include_deleted → unfiltered fetch (row even if soft-deleted);
         * otherwise live rows only. The execution DB layer has no
         * get_live, so a deleted row is mapped to not-found here. */
        execution_t *e = acta_db_execution_get(db, id, &err);

        VLOG(3, "  acta_db_execution_get(%d, include_deleted=%d) → ptr=%p err=%d",
             id, include_deleted, (const void *)e, err);

        if (!include_deleted && e && e->deleted_at) {
            acta_db_execution_free(e);
            return emit_not_found("execution");
        }

        int rc = load_row_or_notfound(db, err, e, id, execution_free_wrap,
                                      "execution get", "execution");
        if (rc)
            return rc;

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

    /* ── delete <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "delete") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_delete, "exec delete", &id))
            return EXIT_INVALID;

        VLOG(1, "exec delete: id=%d", id);
        VLOG(3, "  id=%d db=%p", id, (const void *)db);

        int rc = acta_db_execution_delete(db, id);

        VLOG(3, "  acta_db_execution_delete(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "execution delete");
        }

        VLOG(1, "  deleted exec id=%d", id);
        emit_deleted();
        return EXIT_OK;
    }

    /* ── restore <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "restore") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_restore, "exec restore", &id))
            return EXIT_INVALID;

        VLOG(1, "exec restore: id=%d", id);
        VLOG(3, "  id=%d db=%p", id, (const void *)db);

        int rc = acta_db_execution_restore(db, id);

        VLOG(3, "  acta_db_execution_restore(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return finish_op_error(db, rc, "execution restore");
        }

        VLOG(1, "  restored exec id=%d", id);
        emit_ok_restored(gopts, id);
        return EXIT_OK;
    }

    /* ── start <id> ───────────────────────────────────────────────── */
    if (strcmp(action, "start") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_start, "exec start", &id))
            return EXIT_INVALID;

        VLOG(1, "exec start: id=%d", id);

        int rc = acta_db_execution_start(db, id);
        VLOG(3, "  acta_db_execution_start(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return finish_op_error(db, rc, "execution start");
        }
        VLOG(1, "  started id=%d", id);
        emit_ok_transition(gopts, id, ACTA_EXEC_STATUS_RUNNING);
        return EXIT_OK;
    }

    /* ── cancel <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "cancel") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_cancel, "exec cancel", &id))
            return EXIT_INVALID;

        VLOG(1, "exec cancel: id=%d", id);

        int rc = acta_db_execution_cancel(db, id);
        VLOG(3, "  acta_db_execution_cancel(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return finish_op_error(db, rc, "execution cancel");
        }
        VLOG(1, "  cancelled id=%d", id);
        emit_ok_transition(gopts, id, ACTA_EXEC_STATUS_CANCELLED);
        return EXIT_OK;
    }

    /* ── complete <id> ────────────────────────────────────────────── */
    if (strcmp(action, "complete") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_complete, "exec complete", &id))
            return EXIT_INVALID;

        const char *f_result = cmd_args_flag(ga, "result", 1);

        VLOG(1, "exec complete: id=%d result=%s",
             id, f_result ? f_result : "(null)");

        int rc = acta_db_execution_complete(db, id, f_result);
        VLOG(3, "  acta_db_execution_complete(%d, %p) → rc=%d",
             id, (const void *)f_result, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return finish_op_error(db, rc, "execution complete");
        }
        VLOG(1, "  completed id=%d", id);
        emit_ok_transition(gopts, id, ACTA_EXEC_STATUS_COMPLETED);
        return EXIT_OK;
    }

    /* ── fail <id> ────────────────────────────────────────────────── */
    if (strcmp(action, "fail") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_fail, "exec fail", &id))
            return EXIT_INVALID;

        const char *f_error = cmd_args_flag(ga, "error", 1);

        VLOG(1, "exec fail: id=%d error=%s",
             id, f_error ? f_error : "(null)");

        int rc = acta_db_execution_fail(db, id, f_error);
        VLOG(3, "  acta_db_execution_fail(%d, %p) → rc=%d",
             id, (const void *)f_error, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return finish_op_error(db, rc, "execution fail");
        }
        VLOG(1, "  failed id=%d", id);
        emit_ok_transition(gopts, id, ACTA_EXEC_STATUS_FAILED);
        return EXIT_OK;
    }

    /* ── reset <id> ──────────────────────────────────────────────── */
    if (strcmp(action, "reset") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_reset, "exec reset", &id))
            return EXIT_INVALID;

        VLOG(1, "exec reset: id=%d", id);

        int rc = acta_db_execution_reset(db, id);
        VLOG(3, "  acta_db_execution_reset(%d) → rc=%d", id, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return finish_op_error(db, rc, "execution reset");
        }
        VLOG(1, "  reset id=%d", id);
        emit_ok_transition(gopts, id, ACTA_EXEC_STATUS_PENDING);
        return EXIT_OK;
    }

    /* ── set-raw <id> ─────────────────────────────────────────────── */
    if (strcmp(action, "set-raw") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_set_raw, "exec set-raw", &id))
            return EXIT_INVALID;

        const char *f_raw = NULL;
        if (require_flag(ga, "raw", &f_raw, usage_set_raw, "exec set-raw") < 0)
            return EXIT_INVALID;
        if (!f_raw) {
            VLOG(1, "exec set-raw: ERROR missing required --raw");
            emit_error("missing required flag: --raw");
            usage_set_raw(stderr);
            return EXIT_INVALID;
        }

        VLOG(1, "exec set-raw: id=%d", id);

        int rc = acta_db_execution_set_raw_response(db, id, f_raw);
        VLOG(3, "  acta_db_execution_set_raw_response(%d, %p) → rc=%d",
             id, (const void *)f_raw, rc);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d", rc);
            return finish_op_error(db, rc, "execution set-raw");
        }
        VLOG(1, "  set raw response for id=%d", id);
        /* Status is unchanged by set-raw; echo it so agents can confirm
         * the row's lifecycle state without a follow-up get. */
        int err = 0;
        execution_t *e = acta_db_execution_get(db, id, &err);
        if (e) {
            emit_ok_transition(gopts, id, e->status ? e->status : "");
            acta_db_execution_free(e);
        }
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *f_status   = cmd_args_flag(ga, "status", 1);
        const char *f_ctx_id   = cmd_args_flag(ga, "context_id", 1);
        const char *f_skill_id = cmd_args_flag(ga, "skill_revision_id", 1);
        const char *f_model_id = cmd_args_flag(ga, "model_revision_id", 1);
        const char *f_parent   = cmd_args_flag(ga, "parent_execution_id", 1);

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_list, "exec list") < 0)
            return EXIT_INVALID;

        int ctx_id = 0, skill_rev_id = 0, model_rev_id = 0, parent_id = 0;
        if (parse_nonneg_int_flag(ga, "context_id", &ctx_id, 0,
                                  usage_list, "exec list") < 0 ||
            parse_nonneg_int_flag(ga, "skill_revision_id", &skill_rev_id, 0,
                                  usage_list, "exec list") < 0 ||
            parse_nonneg_int_flag(ga, "model_revision_id", &model_rev_id, 0,
                                  usage_list, "exec list") < 0 ||
            parse_nonneg_int_flag(ga, "parent_execution_id", &parent_id, 0,
                                  usage_list, "exec list") < 0)
            return EXIT_INVALID;

        int include_deleted = cmd_args_has_flag(ga, "include_deleted");
        int full = cmd_args_has_flag(ga, "full");

        execution_query_t q = {
            .status              = f_status,
            .context_id          = ctx_id,
            .skill_revision_id   = skill_rev_id,
            .model_revision_id   = model_rev_id,
            .parent_execution_id = parent_id,
            .include_deleted     = include_deleted,
        };

        VLOG(1, "exec list: status=%s ctx=%d skill_rev=%d model_rev=%d "
                "parent=%d offset=%d limit=%d include_deleted=%d full=%d",
             f_status ? f_status : "(any)",
             q.context_id, q.skill_revision_id, q.model_revision_id,
             q.parent_execution_id, offset, limit, include_deleted, full);

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
                return finish_op_error(db, err, "execution count");
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        /* Default: light projection (no prompt/raw_response/result/error
         * blobs); --full fetches them. */
        int out_count = 0, err = 0;
        execution_t **items = full
            ? acta_db_execution_query(db, &q, offset, limit,
                                      &out_count, &err)
            : acta_db_execution_query_light(db, &q, offset, limit,
                                            &out_count, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  query FAILED err=%d", err);
            acta_db_execution_list_free(items, out_count);
            return finish_op_error(db, err, "execution list");
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
        const char *f_status = cmd_args_flag(ga, "status", 1);

        int ctx_id = 0, skill_rev_id = 0, model_rev_id = 0, parent_id = 0;
        if (parse_nonneg_int_flag(ga, "context_id", &ctx_id, 0,
                                  usage_count, "exec count") < 0 ||
            parse_nonneg_int_flag(ga, "skill_revision_id", &skill_rev_id, 0,
                                  usage_count, "exec count") < 0 ||
            parse_nonneg_int_flag(ga, "model_revision_id", &model_rev_id, 0,
                                  usage_count, "exec count") < 0 ||
            parse_nonneg_int_flag(ga, "parent_execution_id", &parent_id, 0,
                                  usage_count, "exec count") < 0)
            return EXIT_INVALID;

        int include_deleted = cmd_args_has_flag(ga, "include_deleted");

        execution_query_t q = {
            .status              = f_status,
            .context_id          = ctx_id,
            .skill_revision_id   = skill_rev_id,
            .model_revision_id   = model_rev_id,
            .parent_execution_id = parent_id,
            .include_deleted     = include_deleted,
        };

        VLOG(1, "exec count: status=%s ctx=%d skill_rev=%d model_rev=%d "
                "parent=%d include_deleted=%d",
             f_status ? f_status : "(any)",
             q.context_id, q.skill_revision_id,
             q.model_revision_id, q.parent_execution_id, include_deleted);

        VLOG(2, "  q=%p status=%p ctx=%d skill_rev=%d model_rev=%d parent=%d",
             (const void *)&q, (const void *)q.status,
             q.context_id, q.skill_revision_id,
             q.model_revision_id, q.parent_execution_id);

        int err = 0;
        int n = acta_db_execution_count(db, &q, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "execution count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("exec", action, "acta_cli exec help",
                          exec_actions, EXEC_ACTIONS);
}
