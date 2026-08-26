#ifndef ACTA_DB_CLI_COMMAND_H
#define ACTA_DB_CLI_COMMAND_H

#include "json.h"
#include "argparse.h"

#include <acta_db.h>


#include <stdio.h>
#include <string.h>






void version_print(FILE *out);
void help_print(FILE *out);
int tools_print(FILE *out);

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
int cmd_model_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);
int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db);



/* Read everything remaining on stdin into a NUL-terminated malloc'd
* buffer.  Returns NULL on OOM, otherwise the buffer (free it). */
static char *read_stdin_all(void)
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





#endif
