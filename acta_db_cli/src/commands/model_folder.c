#include "commands.h"
#include <string.h>

int cmd_model_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts) {
    (void)ga; (void)gopts;
    fprintf(stderr, "[model-folder.%s] stub\n", action);
    return EXIT_OK;
}
