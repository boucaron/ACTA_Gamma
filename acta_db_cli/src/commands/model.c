#include "commands.h"
#include <string.h>

int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db) {
    (void)ga; (void)gopts;

    if (strcmp(action, "create")  == 0) { fprintf(stderr, "[model.create] stub\n");  return EXIT_OK; }
    if (strcmp(action, "get")     == 0) { fprintf(stderr, "[model.get] stub\n");     return EXIT_OK; }
    if (strcmp(action, "update")  == 0) { fprintf(stderr, "[model.update] stub\n");  return EXIT_OK; }
    if (strcmp(action, "delete")  == 0) { fprintf(stderr, "[model.delete] stub\n");  return EXIT_OK; }
    if (strcmp(action, "restore") == 0) { fprintf(stderr, "[model.restore] stub\n"); return EXIT_OK; }
    if (strcmp(action, "move")    == 0) { fprintf(stderr, "[model.move] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")    == 0) { fprintf(stderr, "[model.list] stub\n");    return EXIT_OK; }
    if (strcmp(action, "count")   == 0) { fprintf(stderr, "[model.count] stub\n");   return EXIT_OK; }

    fprintf(stderr, "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,\"message\":\"unknown model action: %s\"}\n", action);
    return EXIT_CLI;
}
