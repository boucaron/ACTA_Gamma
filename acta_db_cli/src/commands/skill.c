#include "commands.h"
#include "cli_util.h"
#include <string.h>

static const action_def_t skill_actions[] = {
    { "create",  "create a new skill"           },
    { "get",     "fetch a skill by id"          },
    { "update",  "update an existing skill"     },
    { "delete",  "remove a skill"               },
    { "restore", "restore a deleted skill"      },
    { "move",    "move a skill to another folder" },
    { "list",    "list skills"                  },
    { "count",   "count skills"                 },
};
#define SKILL_ACTIONS (sizeof(skill_actions) / sizeof(skill_actions[0]))

int cmd_skill(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    (void)ga; (void)gopts; (void)db;

    if (strcmp(action, "create")  == 0) { fprintf(stderr, "[skill.create] stub\n");  return EXIT_OK; }
    if (strcmp(action, "get")     == 0) { fprintf(stderr, "[skill.get] stub\n");     return EXIT_OK; }
    if (strcmp(action, "update")  == 0) { fprintf(stderr, "[skill.update] stub\n");  return EXIT_OK; }
    if (strcmp(action, "delete")  == 0) { fprintf(stderr, "[skill.delete] stub\n");  return EXIT_OK; }
    if (strcmp(action, "restore") == 0) { fprintf(stderr, "[skill.restore] stub\n"); return EXIT_OK; }
    if (strcmp(action, "move")    == 0) { fprintf(stderr, "[skill.move] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")    == 0) { fprintf(stderr, "[skill.list] stub\n");    return EXIT_OK; }
    if (strcmp(action, "count")   == 0) { fprintf(stderr, "[skill.count] stub\n");   return EXIT_OK; }

    return action_err("skill", action, skill_actions, SKILL_ACTIONS);
}
