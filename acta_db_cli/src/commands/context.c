#include "commands.h"
#include "argparse.h"
#include "cli_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>



/* ── context_t → JSON object ──────────────────────────────────────── */

static void ctx_to_json(FILE *f, const context_t *c, const global_opts_t *gopts)
{
    const char *fl = gopts->fields;  /* NULL = no filter */
    int shown = 0;

    fputc('{', f);

    /* id */
    if (!fl || fields_has(fl, "id")) {
        if (shown++) fputs(", ", f);
        fprintf(f, "\"id\":%d", c->id);
    }
    /* type */
    if ((!fl || fields_has(fl, "type")) && !(gopts->no_nulls && !c->type)) {
        if (shown++) fputs(", ", f);
        fputs("\"type\":", f);
        if (c->type) json_str(f, c->type); else fputs("null", f);
    }
    /* content */
    if ((!fl || fields_has(fl, "content")) && !(gopts->no_nulls && !c->content)) {
        if (shown++) fputs(", ", f);
        fputs("\"content\":", f);
        if (c->content) json_str(f, c->content); else fputs("null", f);
    }
    /* content_hash */
    if ((!fl || fields_has(fl, "content_hash")) && !(gopts->no_nulls && !c->content_hash)) {
        if (shown++) fputs(", ", f);
        fputs("\"content_hash\":", f);
        if (c->content_hash) json_str(f, c->content_hash); else fputs("null", f);
    }
    /* metadata */
    if ((!fl || fields_has(fl, "metadata")) && !(gopts->no_nulls && !c->metadata)) {
        if (shown++) fputs(", ", f);
        fputs("\"metadata\":", f);
        if (c->metadata) json_str(f, c->metadata); else fputs("null", f);
    }
    /* created_at */
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
    /* ── create ───────────────────────────────────────────────────── */
    if (strcmp(action, "create") == 0) {
        const char *type     = cmd_args_flag(ga, "type", 1);
        const char *content  = cmd_args_flag(ga, "content", 1);
        const char *hash     = cmd_args_flag(ga, "hash", 1);
        const char *metadata = cmd_args_flag(ga, "metadata", 1);

        /* --json / --stdin / --from-file takes precedence (handled upstream
         * in gopts->json_input); fall through to flags only if not set */
        if (gopts->json_input) {
            /* TODO: parse gopts->json_input into a context_t */
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"--json input not yet implemented for context\"}\n");
            return EXIT_INVALID;
        }

        if (!type) {
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: type\"}\n");
            return EXIT_INVALID;
        }
        if (!content) {
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing required field: content\"}\n");
            return EXIT_INVALID;
        }

        context_t ctx = {0};
        ctx.type         = (char*) type;
        ctx.content      = (char*) content;
        ctx.content_hash = (char*) hash;
        ctx.metadata     = (char*) metadata;

        int out_id = 0;
        int rc = acta_db_context_create(db, &ctx, &out_id);
        if (rc != ACTA_DB_OK)
            return map_rc_to_exit(rc);

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", out_id);
        } else {
            fprintf(stdout, "{\"id\":%d}\n", out_id);
        }
        return EXIT_OK;
    }

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0)
            return EXIT_INVALID;

        int err = 0;
        context_t *c = acta_db_context_get(db, id, &err);
        if (err != ACTA_DB_OK) {
            acta_db_context_free(c);
            return map_rc_to_exit(err);
        }
        if (!c)
            return EXIT_OK;  /* not found: empty stdout, exit 0 */

        if (gopts->id_only) {
            fprintf(stdout, "%d\n", c->id);
        } else if (gopts->table) {
            ctx_table(stdout, NULL, 1);  /* header */
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
        int limit  = s_lim ? atoi(s_lim) : 0;  /* 0 → C clamps to MAX_PAGE */

        context_query_t q = { .type = f_type, .hash = f_hash };

        /* --count short-circuit (either the global or per-command flag) */
        if (gopts->count || s_count) {
            int err = 0;
            int n = acta_db_context_count(db, &q, &err);
            if (err != ACTA_DB_OK)
                return map_rc_to_exit(err);
            fprintf(stdout, "%d\n", n);
            return EXIT_OK;
        }

        int out_count = 0, err = 0;
        context_t **items = acta_db_context_query(db, &q, offset, limit,
                                                  &out_count, &err);
        if (err != ACTA_DB_OK) {
            acta_db_context_list_free(items, out_count);
            return map_rc_to_exit(err);
        }

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
        int err = 0;
        int n = acta_db_context_count(db, &q, &err);
        if (err != ACTA_DB_OK)
            return map_rc_to_exit(err);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* Unknown action */
    return action_err("context", action, context_actions, CTX_ACTIONS);
}
