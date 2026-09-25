#ifndef ACTA_DB_CLI_COMMAND_H
#define ACTA_DB_CLI_COMMAND_H

#include "json.h"
#include "argparse.h"
#include "cli_util.h"
#include "migrations_sql.h"

#include <acta_db.h>


#include <stdio.h>
#include <string.h>






void version_print(FILE *out);
void help_print(FILE *out);
/* Emit the --tools JSON schema (static data, no DB).  `pretty` = 1
 * switches to 2-space indent; default is single-line compact. */
int tools_print(FILE *out, int pretty);
/* Emit the compact --tools schema (`--tools --compact`): one
 * line per command, ~7 KB.  Static data, no DB. */
int tools_print_compact(FILE *out);

/* Number of positionals declared for "<entity>.<action>" in the tools
 * table; -1 if the action is unknown.  Used by commands_dispatch to
 * reject surplus positionals before the handler runs. */
int tool_entry_positionals(const char *entity, const char *action);

/*
 * P0: entity/action help router (no DB needed).
 *   - action == NULL or "help"   → the whole entity help
 *   - action == known action     → that action's section only
 * Unknown entity or unknown action: emits the canonical single-line
 * JSON error (ACTA_CLI_ERR, code -10) and returns EXIT_CLI.
 */
int entity_help(const char *entity, const char *action, FILE *out);

/* P0: per-entity action→help-section lookup. Prints the section for
 * `action` to `out`; returns 0 if printed, -1 if the action is unknown. */
/* Apply the pending repo-static schema migrations from `migs`
 * (ascending `version`, `n` entries; the caller owns the fresh-vs-
 * foreign user_version-0 table check) — see the definition in
 * src/commands/db.c.  One BEGIN..COMMIT transaction per migration,
 * user_version set after each commit; a failing migration rolls back
 * and the file keeps its prior version (ACTA_DB_ERR_INVALID, with
 * "migration <name> failed: <detail>" in `fail_msg` when non-NULL).
 * *out_version receives the PRAGMA user_version integer (0.1 -> 1) the
 * file is at when the call returns: on success the final applied
 * version; on failure the last successfully applied version (i.e. the
 * prior version, unchanged). */
int db_migrate_apply(db_t *db, const acta_migration_t *migs, int n,
                     int *out_version, char *fail_msg, size_t failmsgsz);

int db_help_for_action(const char *action, FILE *out);
int context_help_for_action(const char *action, FILE *out);
int model_help_for_action(const char *action, FILE *out);
int model_folder_help_for_action(const char *action, FILE *out);
int model_revision_help_for_action(const char *action, FILE *out);
int skill_help_for_action(const char *action, FILE *out);
int skill_folder_help_for_action(const char *action, FILE *out);
int skill_revision_help_for_action(const char *action, FILE *out);
int exec_help_for_action(const char *action, FILE *out);
int log_help_for_action(const char *action, FILE *out);

/* P0: whole-entity help printers (shared by commands.c's entity table). */
void db_usage(FILE *out);
void ctx_usage(FILE *out);
void model_usage(FILE *out);
void model_folder_usage(FILE *out);
void model_revision_usage(FILE *out);
void skill_usage(FILE *out);
void skill_folder_usage(FILE *out);
void skill_rev_usage(FILE *out);
void exec_usage(FILE *out);
void execution_log_usage(FILE *out);

/**
 * Dispatch a parsed (entity, action) pair to its handler.
 *
 * @param entity  e.g. "model", "skill", "execution"
 * @param action  e.g. "create", "list", "get"
 * @param args    positional / flag arguments (owned by caller)
 * @param opts    global options (may carry overrides)
 * @param db      open database handle; valid for the lifetime of this call.
 *                Handlers MUST NOT store it beyond their return.
 *
 * @return 0 on success, non-zero exit code on error.
 */
int commands_dispatch(const char *entity,
                      const char *action,
                      cmd_args_t *args,
                      const global_opts_t *opts,
                      db_t *db);




int cmd_context(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_db(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_execution_log(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_exec(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_model_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_model_revision(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);



/* Read everything remaining on stdin into a NUL-terminated malloc'd
* buffer.  Returns NULL on OOM, otherwise the buffer (free it). */
static inline char *read_stdin_all(void)
{
    size_t cap = 8192;
    size_t len = 0;
    char  *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    for (;;) {
        if (len >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        size_t r = fread(buf + len, 1, cap - len - 1, stdin);
        len += r;
        if (r == 0) break;   /* EOF */
    }
    buf[len] = '\0';
    return buf;
}


/* Read a whole file into a NUL-terminated malloc'd buffer.
 * Returns NULL on open/read/OOM failure; otherwise the buffer (free it). */
static inline char *read_file_all(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    size_t cap = 8192;
    size_t len = 0;
    char  *buf = (char *)malloc(cap);
    if (!buf) { fclose(fp); return NULL; }

    for (;;) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); fclose(fp); return NULL; }
            buf = nb;
        }
        size_t r = fread(buf + len, 1, cap - len - 1, fp);
        len += r;
        if (r == 0) break;   /* EOF */
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}


/* Resolve the JSON input source selected via global flags.
 *
 * The three sources are mutually exclusive; combining two is an error.
 *   --json <blob>     the flag value, verbatim
 *   --from_file <p>   the whole file
 *   --stdin           everything left on stdin
 *
 * Returns 1 and sets *out (NUL-terminated, malloc'd, caller frees) on
 * success; 0 if no source flag was given (*out is NULL — caller should
 * use flag mode); -1 on error (conflicting sources, empty --json value,
 * unreadable file, OOM), in which case the canonical single-line JSON
 * error is already on stderr and the caller should return EXIT_INVALID.
 */
static inline int resolve_input_source(const global_opts_t *g, char **out)
{
    *out = NULL;
    int n_src = (g->json_input != NULL)
              + (g->from_file != NULL)
              + g->from_stdin;
    if (n_src == 0) return 0;

    if (n_src > 1) {
        fprintf(stderr,
            "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
            "\"message\":\"conflicting input sources: --json, --stdin "
            "and --from_file are mutually exclusive\"}\n");
        return -1;
    }

    if (g->json_input) {
        if (g->json_input[0] == '\0') {
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"empty --json value\"}\n");
            return -1;
        }
        size_t n = strlen(g->json_input) + 1;
        char *copy = (char *)malloc(n);
        if (!copy) {
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"out of memory\"}\n");
            return -1;
        }
        memcpy(copy, g->json_input, n);
        *out = copy;
        return 1;
    }

    if (g->from_file) {
        char *file_blob = read_file_all(g->from_file);
        if (!file_blob) {
            fprintf(stderr,
                "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
                "\"message\":\"cannot read file '");
            json_str(stderr, g->from_file);
            fprintf(stderr, "'\"}\n");
            return -1;
        }
        *out = file_blob;
        return 1;
    }

    /* --stdin */
    char *stdin_blob = read_stdin_all();
    if (!stdin_blob) {
        fprintf(stderr,
            "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
            "\"message\":\"failed to read JSON input\"}\n");
        return -1;
    }
    *out = stdin_blob;

    return 1;
}





#endif
