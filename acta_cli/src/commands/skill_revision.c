/* ── skill_revision.c ───────────────────────────────────────────────── */
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
 *   acta_cli skill_revision --help
 */
void skill_rev_usage(FILE *f)
{
    fputs(
"Usage: acta_cli skill_revision <action> [options]\n"
"\n"
"Actions:\n"
"  get         Fetch a skill revision by id\n"
"  get-latest  Fetch the most recent revision for a skill\n"
"  list        List revisions for a skill\n"
"  count       Count revisions for a skill\n"
"  help <action>  Show help for a single action (no arg = full help)\n"
"\n"
"== get <id> ========================================================\n"
"  Fetch a single skill revision by its primary key.\n"
"\n"
"    acta_cli skill_revision get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== get-latest <skill_id> ===========================================\n"
"  Fetch the most recent revision for a given skill.\n"
"\n"
"    acta_cli skill_revision get-latest 7\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== list <skill_id> ================================================\n"
"  List all revisions belonging to a skill.\n"
"\n"
"    acta_cli skill_revision list 7\n"
"    acta_cli skill_revision list 7 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n"
"\n"
"== count <skill_id> ===============================================\n"
"  Count revisions for a skill.\n"
"\n"
"    acta_cli skill_revision count 7\n"
"\n"
"Global options:\n"
"  --table            columnar / plain output instead of JSON\n"
"  --verbose [N]      debug level 0-3 (diagnostics on stderr)\n"
"  --fields <csv>     comma-separated field whitelist\n"
"  --no_nulls         omit null-valued fields from JSON output\n"
"  --id_only          print only the id (get / get-latest)\n"
"\n", f);
}

/* ── per-action usage snippets (printed to stderr on arg errors) ──── */

static void usage_sr_get(FILE *f)
{
    fputs(
"== get <id> ========================================================\n"
"  Fetch a single skill revision by its primary key.\n"
"\n"
"    acta_cli skill_revision get 42\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_sr_latest(FILE *f)
{
    fputs(
"== get-latest <skill_id> ===========================================\n"
"  Fetch the most recent revision for a given skill.\n"
"\n"
"    acta_cli skill_revision get-latest 7\n"
"\n"
"  Options:\n"
"    --id_only            Print only the id\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_sr_list(FILE *f)
{
    fputs(
"== list <skill_id> ================================================\n"
"  List all revisions belonging to a skill.\n"
"\n"
"    acta_cli skill_revision list 7\n"
"    acta_cli skill_revision list 7 --offset 10 --limit 25\n"
"\n"
"  Options:\n"
"    --offset <n>         Skip first N rows (default 0)\n"
"    --limit <n>          Max rows to return (default 0 = unlimited)\n"
"    --count              Return only the row count (no rows)\n"
"    --table              Columnar output instead of JSON\n"
"    --fields <csv>       Comma-separated field filter\n"
"    --no_nulls           Omit null-valued fields from JSON\n", f);
}

static void usage_sr_count(FILE *f)
{
    fputs(
"== count <skill_id> ===============================================\n"
"  Count revisions for a skill.\n"
"\n"
"    acta_cli skill_revision count 7\n", f);
}

/* ── helpers ───────────────────────────────────────────────────────── */

static void vlog_sr_fields(const char *tag, const skill_revision_t *r)
{
    VLOG(2, "%s: id=%d skill_id=%d revision=%d folder_id=%d name=%s "
         "description=%s prompt_template=%s output_schema=%s "
         "created_at=%s updated_at=%s deleted_at=%s",
         tag,
         r->id,
         r->skill_id,
         r->revision,
         r->folder_id,
         r->name             ? r->name             : "(null)",
         r->description      ? r->description      : "(null)",
         r->prompt_template  ? r->prompt_template  : "(null)",
         r->output_schema    ? r->output_schema    : "(null)",
         r->created_at       ? r->created_at       : "(null)",
         r->updated_at       ? r->updated_at       : "(null)",
         r->deleted_at       ? r->deleted_at       : "(null)");
}

static void vlog_sr_raw(const char *tag, const skill_revision_t *r, int rc)
{
    VLOG(3, "%s: sr=%p id=%d rc=%d",
         tag, (const void *)r, r ? r->id : -1, rc);
}

/* free_row adapter for load_row_or_notfound (void* signature). */
static void sr_free_wrap(void *r)
{
    acta_db_skill_revision_free((skill_revision_t *)r);
}

/* ── skill_revision_t → JSON object ───────────────────────────────── */

static void sr_to_json(FILE *f, const skill_revision_t *r, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", r->id);
    }
    if ((!fl || fields_has(fl, "skill_id")) && !(gopts->no_nulls && r->skill_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"skill_id\":%d", r->skill_id);
    }
    if ((!fl || fields_has(fl, "revision")) && !(gopts->no_nulls && r->revision == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"revision\":%d", r->revision);
    }
    if ((!fl || fields_has(fl, "folder_id")) && !(gopts->no_nulls && r->folder_id == 0)) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"folder_id\":%d", r->folder_id);
    }
    if ((!fl || fields_has(fl, "name")) && !(gopts->no_nulls && !r->name)) {
        if (shown++) fputs(", ", f);
        fputs("\"name\":", f);
        if (r->name) json_str(f, r->name); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "description")) && !(gopts->no_nulls && !r->description)) {
        if (shown++) fputs(", ", f);
        fputs("\"description\":", f);
        if (r->description) json_str(f, r->description); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "prompt_template")) && !(gopts->no_nulls && !r->prompt_template)) {
        if (shown++) fputs(", ", f);
        fputs("\"prompt_template\":", f);
        if (r->prompt_template) json_str(f, r->prompt_template); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "output_schema")) && !(gopts->no_nulls && !r->output_schema)) {
        if (shown++) fputs(", ", f);
        fputs("\"output_schema\":", f);
        if (r->output_schema) json_str(f, r->output_schema); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !r->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (r->created_at) json_str(f, r->created_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "updated_at")) && !(gopts->no_nulls && !r->updated_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"updated_at\":", f);
        if (r->updated_at) json_str(f, r->updated_at); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "deleted_at")) && !(gopts->no_nulls && !r->deleted_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"deleted_at\":", f);
        if (r->deleted_at) json_str(f, r->deleted_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void sr_table(FILE *f, const skill_revision_t *r, int header)
{
    if (header) {
        fprintf(f, " %4s  %8s  %8s  %8s  %-20s  %-30s  %-19s  %-19s  %-19s\n",
                "ID", "SKILL", "REV", "FOLDER", "NAME", "DESCRIPTION",
                "CREATED_AT", "UPDATED_AT", "DELETED_AT");
        return;
    }
    char idb[16];
    char skb[16];
    char revb[16];
    char fb[16];
    snprintf(idb,  sizeof idb,  "%d", r->id);
    snprintf(skb,  sizeof skb,  "%d", r->skill_id);
    snprintf(revb, sizeof revb, "%d", r->revision);
    snprintf(fb,   sizeof fb,   "%d", r->folder_id);

    fprintf(f, " %4s  ", idb);
    fprintf(f, " %8s  ", skb);
    fprintf(f, " %8s  ", revb);
    fprintf(f, " %8s  ", fb);
    tcol(f, r->name,          20);
    tcol(f, r->description,   30);
    tcol(f, r->created_at,    19);
    tcol(f, r->updated_at,    19);
    tcol(f, r->deleted_at,    19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t skill_rev_actions[] = {
    { "get",         "fetch a skill revision by id"          },
    { "get-latest",  "fetch the most recent revision for a skill" },
    { "list",        "list revisions for a skill"            },
    { "count",       "count revisions for a skill"           },
    { "help",        "show this help"                        },
};
#define SR_ACTIONS (sizeof(skill_rev_actions) / sizeof(skill_rev_actions[0]))

/* P0: print the help section for one skill_revision action.
 * 0 = printed, -1 = unknown action. */
int skill_revision_help_for_action(const char *action, FILE *out)
{
    if (strcmp(action, "get")        == 0) usage_sr_get(out);
    else if (strcmp(action, "get-latest") == 0) usage_sr_latest(out);
    else if (strcmp(action, "list")     == 0) usage_sr_list(out);
    else if (strcmp(action, "count")    == 0) usage_sr_count(out);
    else return -1;
    return 0;
}

int cmd_skill_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                 db_t *db)
{
    /* ── help: whole entity, or one action via `skill_revision help <action>` ── */
    if (strcmp(action, "help") == 0) {
        const char *sub = cmd_args_next_positional(ga);
        if (sub && strcmp(sub, "help") != 0) {
            if (skill_revision_help_for_action(sub, stdout) == 0)
                return EXIT_OK;
            return unknown_action("skill_revision", sub, "acta_cli skill_revision help",
                                  skill_rev_actions, SR_ACTIONS);
        }
        skill_rev_usage(stdout);
        return EXIT_OK;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", usage_sr_get,
                                 "skill_revision get", &id))
            return EXIT_INVALID;

        VLOG(1, "skill_revision get: fetching id=%d", id);

        int err = 0;
        skill_revision_t *r = acta_db_skill_revision_get(db, id, &err);

        VLOG(3, "  acta_db_skill_revision_get(%d) → ptr=%p err=%d",
             id, (const void *)r, err);

        int rc = load_row_or_notfound(db, err, r, id, sr_free_wrap,
                                      "skill_revision get", "skill_revision");
        if (rc)
            return rc;

        vlog_sr_fields("  result", r);
        vlog_sr_raw("  raw", r, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", r->id);
        } else if (gopts->table) {
            sr_table(stdout, NULL, 1);
            sr_table(stdout, r, 0);
        } else {
            sr_to_json(stdout, r, gopts);
            fputc('\n', stdout);
        }
        acta_db_skill_revision_free(r);
        return EXIT_OK;
    }

    /* ── get-latest <skill_id> ────────────────────────────────────── */
    if (strcmp(action, "get-latest") == 0) {
        int skill_id;
        if (!parse_id_positional(ga, "skill_id", usage_sr_latest,
                                 "skill_revision get-latest", &skill_id))
            return EXIT_INVALID;

        VLOG(1, "skill_revision get-latest: skill_id=%d", skill_id);

        int err = 0;
        skill_revision_t *r = acta_db_skill_revision_get_latest(db, skill_id, &err);

        VLOG(3, "  acta_db_skill_revision_get_latest(%d) → ptr=%p err=%d",
             skill_id, (const void *)r, err);

        int rc = load_row_or_notfound(db, err, r, skill_id, sr_free_wrap,
                                      "skill_revision latest", "skill_revision");
        if (rc)
            return rc;

        vlog_sr_fields("  result", r);
        vlog_sr_raw("  raw", r, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", r->id);
        } else if (gopts->table) {
            sr_table(stdout, NULL, 1);
            sr_table(stdout, r, 0);
        } else {
            sr_to_json(stdout, r, gopts);
            fputc('\n', stdout);
        }
        acta_db_skill_revision_free(r);
        return EXIT_OK;
    }

    /* ── list <skill_id> ──────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        /* KI-6: --id_only is a single-row modifier (get); list actions
         * reject it with exit 4 instead of silently ignoring it. */
        if (gopts->id_only) {
            emit_error("skill_revision list: --id_only is not supported; "
                       "remove the flag (use the JSON rows, --count, or "
                       "--table)");
            return EXIT_INVALID;
        }

        int skill_id;
        if (!parse_id_positional(ga, "skill_id", usage_sr_list,
                                 "skill_revision list", &skill_id))
            return EXIT_INVALID;

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               usage_sr_list, "skill_revision list") < 0)
            return EXIT_INVALID;

        VLOG(1, "skill_revision list: skill_id=%d offset=%d limit=%d",
             skill_id, offset, limit);

        VLOG(2, "  full: skill_id=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             skill_id, offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  skill_id=%d offset=%d limit=%d",
             skill_id, offset, limit);

        if (gopts->count) {
            int err = 0;
            int n = acta_db_skill_revision_count(db, skill_id, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "skill_revision count");
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        skill_revision_t **items = acta_db_skill_revision_list_by_skill(
            db, skill_id, offset, limit, &out_count, &err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  list FAILED err=%d", err);
            acta_db_skill_revision_list_free(items, out_count);
            return finish_op_error(db, err, "skill_revision list");
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_sr_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            sr_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                sr_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                sr_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_skill_revision_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count <skill_id> ─────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        int skill_id;
        if (!parse_id_positional(ga, "skill_id", usage_sr_count,
                                 "skill_revision count", &skill_id))
            return EXIT_INVALID;

        VLOG(1, "skill_revision count: skill_id=%d", skill_id);

        VLOG(2, "  skill_id=%d", skill_id);

        int err = 0;
        int n = acta_db_skill_revision_count(db, skill_id, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "skill_revision count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("skill_revision", action, "acta_cli skill_revision help",
                          skill_rev_actions, SR_ACTIONS);
}
