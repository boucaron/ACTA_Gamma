#include "commands.h"
#include <string.h>

int cmd_model_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db) {
    (void)ga; (void)gopts;
    fprintf(stderr, "[model-rev.%s] stub\n", action);
    return EXIT_OK;
}
