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
 *  Usage: set vlog_gopts at the top of cmd_context, then call
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
    { "count",  "count contexts" }
};
#define CTX_ACTIONS (sizeof(context_actions) / sizeof(context_actions[0]))

int cmd_context(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        const char *type     = cmd_args_flag(ga, "type", 1);
        const char *content  = cmd_args_flag(ga, "content", 1);
        const char *hash     = cmd_args_flag(ga, "hash", 1);
        const char *metadata = cmd_args_flag(ga, "metadata", 1);

        VLOG(1, "context create: type=%s content=%s",
             type    ? type    : "(missing)",
             content ? content : "(missing)");

        VLOG(2, "  params: type=%s content=%s hash=%s metadata=%s fields=%s "
                "no_nulls=%d id_only=%d table=%d",
             type     ? type     : "(null)",
             content  ? content  : "(null)",
             hash     ? hash     : "(null)",
             metadata ? metadata : "(null)",
             gopts->fields ? gopts->fields : "(all)",
             gopts->no_nulls, gopts->id_only, gopts->table);

        VLOG(3, "  raw: ga=%p type=%p content=%p hash=%p metadata=%p",
             (const void *)ga, (const void *)type,
             (const void *)content, (const void *)hash,
             (const void *)metadata);

        if (gopts->json_input) {
            VLOG(1, "  using --json input (not yet implemented)");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"--json input not yet implemented for context\"}\n");
            return EXIT_INVALID;
        }

        if (!type) {
            VLOG(1, "  ERROR: missing required field 'type'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: type\"}\n");
            return EXIT_INVALID;
        }
        if (!content) {
            VLOG(1, "  ERROR: missing required field 'content'");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: content\"}\n");
            return EXIT_INVALID;
        }

        context_t ctx = {0};
        ctx.type         = (char *)type;
        ctx.content      = (char *)content;
        ctx.content_hash = (char *)hash;
        ctx.metadata     = (char *)metadata;

        vlog_ctx_fields("  pre-create", &ctx);
        VLOG(3, "  ctx=%p &out_id=%p",
             (const void *)&ctx, (const void *)&ctx.id);

        int out_id = 0;
        int rc = acta_db_context_create(db, &ctx, &out_id);

        VLOG(3, "  acta_db_context_create → rc=%d out_id=%d", rc, out_id);

        if (rc != ACTA_DB_OK) {
            VLOG(1, "  FAILED rc=%d → exit mapping", rc);
            return map_rc_to_exit(rc);
        }

        VLOG(1, "  created context id=%d", out_id);

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
            VLOG(1, "context get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "context get: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "context get: fetching id=%d", id);

        int err = 0;
        context_t *c = acta_db_context_get(db, id, &err);

        VLOG(3, "  acta_db_context_get(%d) → ptr=%p err=%d",
             id, (const void *)c, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_context_free(c);
            return map_rc_to_exit(err);
        }
        if (!c) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

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
        const char *f_type  = cmd_args_flag(ga, "type", 1);
        const char *f_hash  = cmd_args_flag(ga, "hash", 1);
        const char *s_off   = cmd_args_flag(ga, "offset", 1);
        const char *s_lim   = cmd_args_flag(ga, "limit", 1);
        const char *s_count = cmd_args_flag(ga, "count", 0);

        int offset = s_off ? atoi(s_off) : 0;
        int limit  = s_lim ? atoi(s_lim) : 0;

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

        if (gopts->count || s_count) {
            int err = 0;
            int n = acta_db_context_count(db, &q, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
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
            return map_rc_to_exit(err);
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
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "context: unknown action '%s'", action ? action : "(null)");
    return action_err("context", action, context_actions, CTX_ACTIONS);
}
