#include "commands.h"
#include "cli_util.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  cmd_exec                                                           */
/* ------------------------------------------------------------------ */

static const action_def_t exec_actions[] = {
    { "create",   "create a new exec record"        },
    { "get",      "fetch an exec by id"             },
    { "start",    "mark exec as started"            },
    { "cancel",   "cancel a running exec"           },
    { "complete", "mark exec as completed"          },
    { "fail",     "mark exec as failed"             },
    { "set-raw",  "attach raw output to an exec"    },
    { "list",     "list execs"                      },
    { "count",    "count execs"                     },
};
#define EXEC_ACTIONS (sizeof(exec_actions) / sizeof(exec_actions[0]))

int cmd_exec(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
             db_t *db)
{
    (void)ga; (void)gopts; (void)db;

    if (strcmp(action, "create")   == 0) { fprintf(stderr, "[exec.create] stub\n");    return EXIT_OK; }
    if (strcmp(action, "get")      == 0) { fprintf(stderr, "[exec.get] stub\n");       return EXIT_OK; }
    if (strcmp(action, "start")    == 0) { fprintf(stderr, "[exec.start] stub\n");     return EXIT_OK; }
    if (strcmp(action, "cancel")   == 0) { fprintf(stderr, "[exec.cancel] stub\n");    return EXIT_OK; }
    if (strcmp(action, "complete") == 0) { fprintf(stderr, "[exec.complete] stub\n");  return EXIT_OK; }
    if (strcmp(action, "fail")     == 0) { fprintf(stderr, "[exec.fail] stub\n");      return EXIT_OK; }
    if (strcmp(action, "set-raw")  == 0) { fprintf(stderr, "[exec.set-raw] stub\n");   return EXIT_OK; }
    if (strcmp(action, "list")     == 0) { fprintf(stderr, "[exec.list] stub\n");      return EXIT_OK; }
    if (strcmp(action, "count")    == 0) { fprintf(stderr, "[exec.count] stub\n");     return EXIT_OK; }

    return action_err("exec", action, exec_actions, EXEC_ACTIONS);
}
