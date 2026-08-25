#include "commands.h"
#include "cli_util.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  cmd_log                                                            */
/* ------------------------------------------------------------------ */

static const action_def_t log_actions[] = {
    { "create", "create a new log entry"     },
    { "get",    "fetch a log entry by id"    },
    { "list",   "list log entries"           },
    { "count",  "count log entries"          },
};
#define LOG_ACTIONS (sizeof(log_actions) / sizeof(log_actions[0]))

int cmd_log(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
            db_t *db)
{
    (void)ga; (void)gopts; (void)db;

    if (strcmp(action, "create") == 0) { fprintf(stderr, "[log.create] stub\n"); return EXIT_OK; }
    if (strcmp(action, "get")    == 0) { fprintf(stderr, "[log.get] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")   == 0) { fprintf(stderr, "[log.list] stub\n");   return EXIT_OK; }
    if (strcmp(action, "count")  == 0) { fprintf(stderr, "[log.count] stub\n");  return EXIT_OK; }

    return action_err("log", action, log_actions, LOG_ACTIONS);
}
