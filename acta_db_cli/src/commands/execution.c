#include "commands.h"
#include <string.h>

int cmd_exec(const char *action, cmd_args_t *ga, const global_opts_t *gopts) {
    (void)ga; (void)gopts;

    if (strcmp(action, "create")     == 0) { fprintf(stderr, "[exec.create] stub\n");     return EXIT_OK; }
    if (strcmp(action, "get")        == 0) { fprintf(stderr, "[exec.get] stub\n");        return EXIT_OK; }
    if (strcmp(action, "start")      == 0) { fprintf(stderr, "[exec.start] stub\n");      return EXIT_OK; }
    if (strcmp(action, "cancel")     == 0) { fprintf(stderr, "[exec.cancel] stub\n");     return EXIT_OK; }
    if (strcmp(action, "complete")   == 0) { fprintf(stderr, "[exec.complete] stub\n");   return EXIT_OK; }
    if (strcmp(action, "fail")       == 0) { fprintf(stderr, "[exec.fail] stub\n");       return EXIT_OK; }
    if (strcmp(action, "set-raw")    == 0) { fprintf(stderr, "[exec.set-raw] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")       == 0) { fprintf(stderr, "[exec.list] stub\n");       return EXIT_OK; }
    if (strcmp(action, "count")      == 0) { fprintf(stderr, "[exec.count] stub\n");      return EXIT_OK; }

    fprintf(stderr, "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,\"message\":\"unknown exec action: %s\"}\n", action);
    return EXIT_CLI;
}
