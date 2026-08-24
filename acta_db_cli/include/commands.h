#include "json.h"
#include "argparse.h"

#include <stdio.h>
#include <string.h>






void version_print(FILE *out);
void help_print(FILE *out);
int tools_print(FILE *out);

int commands_dispatch(const char *entity, const char *action,
                      cmd_args_t *ga, const global_opts_t *gopts);




int cmd_context(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_db(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_log(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_exec(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_model_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_model_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_skill_folder(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_skill_rev(const char *action, cmd_args_t *ga, const global_opts_t *gopts);
int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts);






