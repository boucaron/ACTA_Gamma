#include "commands.h"
#include <string.h>

int cmd_skill_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts) {
    (void)ga; (void)gopts;
    fprintf(stderr, "[skill-rev.%s] stub\n", action);
    return EXIT_OK;
}
