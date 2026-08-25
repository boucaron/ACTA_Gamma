#include "commands.h"
#include "cli_util.h"
#include <string.h>

/*
 * db entity — stub handlers.
 * Real implementation will call acta_db_open, acta_db_exec, etc.
 */

static const action_def_t db_actions[] = {
    { "exec",    "execute raw SQL"               },
    { "version", "print SQLite library version"  },
};
#define DB_ACTIONS (sizeof(db_actions) / sizeof(db_actions[0]))

int cmd_db(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
           db_t *db)
{
    (void)ga; (void)gopts; (void)db;

    if (strcmp(action, "exec") == 0)    { fprintf(stderr, "[db.exec] stub\n");    return EXIT_OK; }
    if (strcmp(action, "version") == 0) { fprintf(stderr, "[db.version] stub\n"); return EXIT_OK; }

    return action_err("db", action, db_actions, DB_ACTIONS);
}

