#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════ */
/*  Usage / help                                                       */
/* ══════════════════════════════════════════════════════════════════ */

/* Non-static: the global dispatch layer can call this to handle
 *   actagamma_db context --help   without re-parsing the subcommand.          */
void ctx_usage(FILE *f)
{
    fputs(
"Usage: actagamma_db context <action> [options]\n"
"\n"
"Actions:\n"
"  create   Create a new context\n"
"  get      Fetch a context by id\n"
"  list     List contexts (filterable, paginated)\n"
"  count    Count contexts (filterable)\n"
"  help     Show this help\n"
"\n"
"== create =========================================================\n"
"  actagamma_db context create --type <T> --content <C> --hash <H>\n"
"                     [--metadata <M>]\n"
"\n"
"  Or pipe a JSON body from stdin:\n"
"  echo '{\"type\":\"s\",\"content\":\"hi\",\"content_hash\":\"ab\"}' \\\n"
"      | actagamma_db context create --stdin\n"
"\n"
"  Required (via flags or JSON key):\n"
"    --type <string>          context type   (JSON key: \"type\")\n"
"    --content <string>       payload        (JSON key: \"content\")\n"
"    --hash <string>          content hash   (JSON key: \"content_hash\")\n"
"  Optional:\n"
"    --metadata <string>      extra data     (JSON key: \"metadata\")\n"
"\n"
"== get ============================================================\n"
"  actagamma_db context get <positive-integer-id>\n"
"  Example:\n"
"    actagamma_db context get 42\n"
"  Options:\n"
"    --table          column output instead of JSON\n"
"    --id_only        print just the numeric id\n"
"    --fields <a,b>   restrict output fields (comma-separated)\n"
"    --no_nulls       omit fields that are null\n"
"\n"
"== list ============================================================\n"
"  actagamma_db context list [--type <T>] [--hash <H>]\n"
"                 [--offset <int>] [--limit <int>] [--count]\n"
"  Options:\n"
"    --type <string>      filter by type\n"
"    --hash <string>      filter by content hash\n"
"    --offset <int>       skip first N results (default 0)\n"
"    --limit <int>        max results (0 or omitted = unlimited)\n"
"    --count              print total match count instead of items\n"
"    --table              column output\n"
"    --fields <a,b>       restrict output fields\n"
"    --no_nulls           omit null fields\n"
"\n"
"== count ===========================================================\n"
"  actagamma_db context count [--type <T>] [--hash <H>]\n"
"  Prints a single integer: the number of matching contexts.\n"
"\n"
"Global options (apply to every action):\n"
"  --json <blob>    read input from a JSON object (flag value)\n"
"  --stdin          read input from stdin as JSON\n"
"  --from_file <p>  read input from a file as JSON\n"
"  --fields <a,b>   comma-separated field filter for output\n"
"  --no_nulls       suppress null-valued fields in JSON output\n"
"  --id_only        print only the numeric id\n"
"  --table          columnar output instead of JSON\n"
"  --verbose <n>    debug level 0-3 (diagnostics on stderr)\n"
"\n", f);

}



/* ══════════════════════════════════════════════════════════════════ */
/*  helpers                                                            */
/* ══════════════════════════════════════════════════════════════════ */

static void vlog_ctx_fields(const char *tag, const context_t *c)
{
    VLOG(2, "%s: id=%d type=%s content=%s hash=%s metadata=%s created_at=%s",
         tag,
         c->id,
         c->type         ? c->type         : "(null)",
         c->content      ? c->content      : "(null)",
         c->content_hash ? c->content_hash : "(null)",
         c->metadata     ? c->metadata     : "(null)",
         c->created_at   ? c->created_at   : "(null)");
}

static void vlog_ctx_raw(const char *tag, const context_t *c, int rc)
{
    VLOG(3, "%s: ctx=%p id=%d rc=%d",
         tag, (const void *)c, c ? c->id : -1, rc);
}

/* free_row adapter for load_row_or_notfound (void* signature). */
static void ctx_free_wrap(void *c)
{
    acta_db_context_free((context_t *)c);
}

/* ── context_t → JSON object ──────────────────────────────────────── */

static void ctx_to_json(FILE *f, const context_t *c, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", c->id);
    }
    if ((!fl || fields_has(fl, "type")) && !(gopts->no_nulls && !c->type)) {
        if (shown++) fputs(", ", f);
        fputs("\"type\":", f);
        if (c->type) json_str(f, c->type); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "content")) && !(gopts->no_nulls && !c->content)) {
        if (shown++) fputs(", ", f);
        fputs("\"content\":", f);
        if (c->content) json_str(f, c->content); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "content_hash")) && !(gopts->no_nulls && !c->content_hash)) {
        if (shown++) fputs(", ", f);
        fputs("\"content_hash\":", f);
        if (c->content_hash) json_str(f, c->content_hash); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "metadata")) && !(gopts->no_nulls && !c->metadata)) {
        if (shown++) fputs(", ", f);
        fputs("\"metadata\":", f);
        if (c->metadata) json_str(f, c->metadata); else fputs("null", f);
    }
    if ((!fl || fields_has(fl, "created_at")) && !(gopts->no_nulls && !c->created_at)) {
        if (shown++) fputs(", ", f);
        fputs("\"created_at\":", f);
        if (c->created_at) json_str(f, c->created_at); else fputs("null", f);
    }

    fputc('}', f);
}

/* ── --table ───────────────────────────────────────────────────────── */

static void ctx_table(FILE *f, const context_t *c, int header)
{
    if (header) {
        fprintf(f, " %4s  %-10s  %-38s  %-10s  %-38s  %-19s\n",
                "ID", "TYPE", "CONTENT", "HASH", "METADATA", "CREATED_AT");
        return;
    }
    char idb[16];
    snprintf(idb, sizeof idb, "%d", c->id);
    fprintf(f, " %4s  ", idb);
    tcol(f, c->type,         10);
    tcol(f, c->content,      38);
    tcol(f, c->content_hash, 10);
    tcol(f, c->metadata,     38);
    tcol(f, c->created_at,   19);
    fputc('\n', f);
}

/* ══════════════════════════════════════════════════════════════════ */
/*  Dispatch                                                           */
/* ══════════════════════════════════════════════════════════════════ */

static const action_def_t context_actions[] = {
    { "create", "create a new context" },
    { "get",    "fetch a context by id" },
    { "list",   "list all contexts" },
    { "count",  "count contexts" },
    { "help",   "show this help" }
};
#define CTX_ACTIONS (sizeof(context_actions) / sizeof(context_actions[0]))

int cmd_context(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                db_t *db)
{
    /* ── help (subcommand-level; only the bare word "help") ─────── */
    if (strcmp(action, "help") == 0) {
        ctx_usage(stdout);
        return EXIT_OK;
    }

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        context_t ctx = {0};
        int json_owned = 0;
        int ret = EXIT_OK;

        /* ── populate ctx ── */
        char *blob = NULL;
        int src = resolve_input_source(gopts, &blob);
        if (src < 0) {
            ctx_usage(stderr);
            return EXIT_INVALID;   /* error line already on stderr */
        }
        if (src) {
            VLOG(1, "context create: JSON input (%zu bytes)", strlen(blob));

            if (json_parse_context(blob, &ctx) != 0) {
                VLOG(1, "  JSON parse error");
                emit_error("invalid JSON body");
                ctx_usage(stderr);
                free(blob);
                return EXIT_INVALID;
            }
            free(blob);
            json_owned = 1;
        } else {
            ctx.type         = (char *)cmd_args_flag(ga, "type", 1);
            ctx.content      = (char *)cmd_args_flag(ga, "content", 1);
            ctx.content_hash = (char *)cmd_args_flag(ga, "hash", 1);
            ctx.metadata     = (char *)cmd_args_flag(ga, "metadata", 1);

            /* All four NULL → likely a typo in a flag name.  Nudge
               before the per-field errors below. */
            if (!ctx.type && !ctx.content &&
                !ctx.content_hash && !ctx.metadata) {
                fprintf(stderr,
                    "Warning: no recognised flags for 'create'.\n"
                    "  Did you misspell a flag?  Expected:\n"
                    "    --type  --content  --hash  --metadata\n"
                    "  Or use --json <blob>, --stdin, or --from_file <path> for a JSON body.\n"
                    "  Run 'actagamma_db context help' for full usage.\n");
            }
        }

        /* ── VLOGs now read from ctx (covers both paths) ── */
        VLOG(1, "context create: type=%s content=%s",
             ctx.type    ? ctx.type    : "(missing)",
             ctx.content ? ctx.content : "(missing)");

        VLOG(2, "  params: type=%s content=%s hash=%s metadata=%s fields=%s "
                "no_nulls=%d id_only=%d table=%d",
             ctx.type     ? ctx.type     : "(null)",
             ctx.content  ? ctx.content  : "(null)",
             ctx.content_hash ? ctx.content_hash : "(null)",
             ctx.metadata ? ctx.metadata : "(null)",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  ctx=%p json_owned=%d", (const void *)&ctx, json_owned);

        /* ── required-field validation (uses ctx, not locals) ── */
        {
            struct { const char *field; const char *val; } reqs[] = {
                { "type",    ctx.type         },
                { "content", ctx.content      },
                { "hash",    ctx.content_hash },
            };
            for (size_t i = 0; i < sizeof reqs / sizeof reqs[0]; i++) {
                if (!reqs[i].val) {
                    VLOG(1, "  ERROR: missing required field '%s'",
                         reqs[i].field);
                    char msg[96];
                    snprintf(msg, sizeof msg,
                             "missing required field: %s", reqs[i].field);
                    emit_error(msg);
                    ctx_usage(stderr);
                    ret = EXIT_INVALID;
                    goto cleanup_create;
                }
            }
        }

        vlog_ctx_fields("  pre-create", &ctx);

        int out_id = 0;
        int rc = acta_db_context_create(db, &ctx, &out_id);
        VLOG(3, "  acta_db_context_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            ret = finish_op_error(db, rc, "context create");
            goto cleanup_create;
        }

        VLOG(1, "  created context id=%d", out_id);
        emit_ok_id(gopts, out_id);

        ret = EXIT_OK;
        goto cleanup_create;

    cleanup_create:
        if (json_owned) {
            free(ctx.type);
            free(ctx.content);
            free(ctx.content_hash);
            free(ctx.metadata);
            free(ctx.created_at);
        }
        return ret;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        int id;
        if (!parse_id_positional(ga, "id", ctx_usage, "context get", &id))
            return EXIT_INVALID;

        VLOG(1, "context get: fetching id=%d", id);

        int err = 0;
        context_t *c = acta_db_context_get(db, id, &err);

        VLOG(3, "  acta_db_context_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        int rc = load_row_or_notfound(db, err, c, id, ctx_free_wrap,
                                      "context get", "context");
        if (rc)
            return rc;

        vlog_ctx_fields("  result", c);
        vlog_ctx_raw("  raw", c, 0);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", c->id);
        } else if (gopts->table) {
            ctx_table(stdout, NULL, 1);
            ctx_table(stdout, c, 0);
        } else {
            ctx_to_json(stdout, c, gopts);
            fputc('\n', stdout);
        }
        acta_db_context_free(c);
        return EXIT_OK;
    }

    /* ── list ─────────────────────────────────────────────────────── */
    if (strcmp(action, "list") == 0) {
        const char *f_type = cmd_args_flag(ga, "type", 1);
        const char *f_hash = cmd_args_flag(ga, "hash", 1);

        int offset = 0, limit = 0;
        if (parse_offset_limit(ga, &offset, &limit,
                               ctx_usage, "context list") < 0)
            return EXIT_INVALID;

        context_query_t q = { .type = f_type, .hash = f_hash };

        VLOG(1, "context list: type=%s hash=%s offset=%d limit=%d",
             f_type ? f_type : "(any)",
             f_hash ? f_hash : "(any)",
             offset, limit ? limit : 0);

        VLOG(2, "  full: type=%s hash=%s offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             f_type ? f_type : "(null)",
             f_hash ? f_hash : "(null)",
             offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  q=%p q.type=%p q.hash=%p",
             (const void *)&q, (const void *)q.type, (const void *)q.hash);

        if (gopts->count) {
            int err = 0;
            int n = acta_db_context_count(db, &q, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return finish_op_error(db, err, "context count");
            }
            VLOG(1, "  count=%d", n);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        context_t **items = acta_db_context_query(db, &q, offset, limit,
                                                  &out_count, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  query FAILED err=%d", err);
            acta_db_context_list_free(items, out_count);
            return finish_op_error(db, err, "context list");
        }

        VLOG(1, "  %d item(s) returned", out_count);

        for (int i = 0; i < out_count; i++)
            vlog_ctx_fields("  item", items[i]);

        VLOG(3, "  items=%p count=%d",
             (const void *)items, out_count);

        if (gopts->table) {
            ctx_table(stdout, NULL, 1);
            for (int i = 0; i < out_count; i++)
                ctx_table(stdout, items[i], 0);
        } else if (out_count == 0) {
            fprintf(stdout, "[]\n");
        } else {
            fputc('[', stdout);
            for (int i = 0; i < out_count; i++) {
                if (i) fputs(", ", stdout);
                ctx_to_json(stdout, items[i], gopts);
            }
            fputc(']', stdout);
            fputc('\n', stdout);
        }
        acta_db_context_list_free(items, out_count);
        return EXIT_OK;
    }

    /* ── count ────────────────────────────────────────────────────── */
    if (strcmp(action, "count") == 0) {
        const char *f_type = cmd_args_flag(ga, "type", 1);
        const char *f_hash = cmd_args_flag(ga, "hash", 1);

        context_query_t q = { .type = f_type, .hash = f_hash };

        VLOG(1, "context count: type=%s hash=%s",
             f_type ? f_type : "(any)",
             f_hash ? f_hash : "(any)");

        VLOG(2, "  q.type=%p q.hash=%p",
             (const void *)q.type, (const void *)q.hash);

        int err = 0;
        int n = acta_db_context_count(db, &q, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return finish_op_error(db, err, "context count");
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* ── Unknown action: suggest closest match + pointer to help ── */
    return unknown_action("context", action, "actagamma_db context help",
                          context_actions, CTX_ACTIONS);
}
