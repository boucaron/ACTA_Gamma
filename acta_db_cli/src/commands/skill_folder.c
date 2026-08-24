#include "commands.h"
#include <string.h>

int cmd_skill_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db) {
    (void)ga; (void)gopts;
    fprintf(stderr, "[skill-folder.%s] stub\n", action);
    return EXIT_OK;
}
