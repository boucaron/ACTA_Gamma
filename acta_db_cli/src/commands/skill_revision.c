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
 *  Usage: set vlog_gopts at the top of cmd_skill_rev, then call
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
    { "get",    "fetch a skill revision by id"           },
    { "latest", "fetch the latest revision for a skill"  },
    { "list",   "list revisions for a skill"             },
    { "count",  "count revisions for a skill"            }
};
#define SR_ACTIONS (sizeof(skill_rev_actions) / sizeof(skill_rev_actions[0]))

int cmd_skill_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                 db_t *db)
{
    vlog_gopts = gopts;   /* ← make VLOG() see the current verbose level */

    /* ── get <id> ─────────────────────────────────────────────────── */
    if (strcmp(action, "get") == 0) {
        const char *id_str = cmd_args_next_positional(ga);
        if (!id_str) {
            VLOG(1, "skill-rev get: ERROR missing <id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <id>\"}\n");
            return EXIT_INVALID;
        }
        int id = atoi(id_str);
        if (id <= 0) {
            VLOG(1, "skill-rev get: invalid id=%s", id_str);
            return EXIT_INVALID;
        }

        VLOG(1, "skill-rev get: fetching id=%d", id);

        int err = 0;
        skill_revision_t *r = acta_db_skill_revision_get(db, id, &err);

        VLOG(3, "  acta_db_skill_revision_get(%d) → ptr=%p err=%d",
             id, (const void *)r, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_skill_revision_free(r);
            return map_rc_to_exit(err);
        }
        if (!r) {
            VLOG(1, "  not found (id=%d)", id);
            return EXIT_OK;
        }

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

    /* ── latest <skill_id> ────────────────────────────────────────── */
    if (strcmp(action, "latest") == 0) {
        const char *skill_str = cmd_args_next_positional(ga);
        if (!skill_str) {
            VLOG(1, "skill-rev latest: ERROR missing <skill_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <skill_id>\"}\n");
            return EXIT_INVALID;
        }
        int skill_id = atoi(skill_str);
        if (skill_id <= 0) {
            VLOG(1, "skill-rev latest: invalid skill_id=%s", skill_str);
            return EXIT_INVALID;
        }

        VLOG(1, "skill-rev latest: skill_id=%d", skill_id);

        int err = 0;
        skill_revision_t *r = acta_db_skill_revision_get_latest(db, skill_id, &err);

        VLOG(3, "  acta_db_skill_revision_get_latest(%d) → ptr=%p err=%d",
             skill_id, (const void *)r, err);

        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d → exit mapping", err);
            acta_db_skill_revision_free(r);
            return map_rc_to_exit(err);
        }
        if (!r) {
            VLOG(1, "  no revisions found (skill_id=%d)", skill_id);
            return EXIT_OK;
        }

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
        const char *skill_str = cmd_args_next_positional(ga);
        if (!skill_str) {
            VLOG(1, "skill-rev list: ERROR missing <skill_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <skill_id>\"}\n");
            return EXIT_INVALID;
        }
        int skill_id = atoi(skill_str);
        if (skill_id <= 0) {
            VLOG(1, "skill-rev list: invalid skill_id=%s", skill_str);
            return EXIT_INVALID;
        }

        const char *s_off   = cmd_args_flag(ga, "offset", 1);
        const char *s_lim   = cmd_args_flag(ga, "limit", 1);
        const char *s_count = cmd_args_flag(ga, "count", 0);

        int offset = 0, limit = 0;

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
            limit = (int)v;   /* 0 = no limit (documented) */
        }

        VLOG(1, "skill-rev list: skill_id=%d offset=%d limit=%d",
             skill_id, offset, limit);

        VLOG(2, "  full: skill_id=%d offset=%d limit=%d "
                "no_nulls=%d table=%d fields=%s",
             skill_id, offset, limit,
             gopts->no_nulls, gopts->table,
             gopts->fields ? gopts->fields : "(all)");

        VLOG(3, "  skill_id=%d offset=%d limit=%d",
             skill_id, offset, limit);

        if (gopts->count || s_count) {
            int err = 0;
            int n = acta_db_skill_revision_count(db, skill_id, &err);
            if (err != ACTA_DB_OK) {
                VLOG(1, "  count FAILED err=%d", err);
                return map_rc_to_exit(err);
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
            return map_rc_to_exit(err);
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
        const char *skill_str = cmd_args_next_positional(ga);
        if (!skill_str) {
            VLOG(1, "skill-rev count: ERROR missing <skill_id>");
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"missing positional: <skill_id>\"}\n");
            return EXIT_INVALID;
        }
        int skill_id = atoi(skill_str);
        if (skill_id <= 0) {
            VLOG(1, "skill-rev count: invalid skill_id=%s", skill_str);
            return EXIT_INVALID;
        }

        VLOG(1, "skill-rev count: skill_id=%d", skill_id);

        VLOG(2, "  skill_id=%d", skill_id);

        int err = 0;
        int n = acta_db_skill_revision_count(db, skill_id, &err);
        if (err != ACTA_DB_OK) {
            VLOG(1, "  FAILED err=%d", err);
            return map_rc_to_exit(err);
        }
        VLOG(1, "  result: %d", n);
        fprintf(stdout, "%d\n", n);
        return EXIT_OK;
    }

    /* Unknown action */
    VLOG(1, "skill-rev: unknown action '%s'", action ? action : "(null)");
    return action_err("skill-rev", action, skill_rev_actions, SR_ACTIONS);
}
