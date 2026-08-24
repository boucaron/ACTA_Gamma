#include "commands.h"
#include <string.h>

int cmd_log(const char *action, cmd_args_t *ga, const global_opts_t *gopts) {
    (void)ga; (void)gopts;

    if (strcmp(action, "create") == 0) { fprintf(stderr, "[log.create] stub\n"); return EXIT_OK; }
    if (strcmp(action, "get")    == 0) { fprintf(stderr, "[log.get] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")   == 0) { fprintf(stderr, "[log.list] stub\n");   return EXIT_OK; }
    if (strcmp(action, "count")  == 0) { fprintf(stderr, "[log.count] stub\n");  return EXIT_OK; }

    fprintf(stderr, "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,\"message\":\"unknown log action: %s\"}\n", action);
    return EXIT_CLI;
}
