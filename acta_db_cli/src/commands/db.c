#include "commands.h"
#include <string.h>

/*
 * db entity — stub handlers.
 * Real implementation will call acta_db_open, acta_db_exec, etc.
 */

int cmd_db(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db) {
    (void)ga; (void)gopts;

    if (strcmp(action, "open") == 0) {
        fprintf(stderr, "[db.open] stub\n");
        return EXIT_OK;
    }
    if (strcmp(action, "exec") == 0) {
        fprintf(stderr, "[db.exec] stub\n");
        return EXIT_OK;
    }
    if (strcmp(action, "begin") == 0)  { fprintf(stderr, "[db.begin] stub\n");  return EXIT_OK; }
    if (strcmp(action, "commit") == 0) { fprintf(stderr, "[db.commit] stub\n"); return EXIT_OK; }
    if (strcmp(action, "rollback") == 0){fprintf(stderr, "[db.rollback] stub\n");return EXIT_OK; }
    if (strcmp(action, "version") == 0){fprintf(stderr, "[db.version] stub\n");return EXIT_OK; }

    fprintf(stderr, "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,\"message\":\"unknown db action: %s\"}\n", action);
    return EXIT_CLI;
}
