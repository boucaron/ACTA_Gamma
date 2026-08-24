#include "commands.h"
#include <string.h>

int cmd_context(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db) {
    (void)ga; (void)gopts;

    if (strcmp(action, "create") == 0) { fprintf(stderr, "[context.create] stub\n"); return EXIT_OK; }
    if (strcmp(action, "get")    == 0) { fprintf(stderr, "[context.get] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")   == 0) { fprintf(stderr, "[context.list] stub\n");   return EXIT_OK; }
    if (strcmp(action, "count")  == 0) { fprintf(stderr, "[context.count] stub\n");  return EXIT_OK; }

    fprintf(stderr, "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,\"message\":\"unknown context action: %s\"}\n", action);
    return EXIT_CLI;
}
