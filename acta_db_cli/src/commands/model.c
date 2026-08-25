#include "commands.h"
#include "cli_util.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  cmd_model                                                          */
/* ------------------------------------------------------------------ */

static const action_def_t model_actions[] = {
    { "create",  "create a new model"           },
    { "get",     "fetch a model by id"          },
    { "update",  "update an existing model"     },
    { "delete",  "remove a model"               },
    { "restore", "restore a deleted model"      },
    { "move",    "move a model to another folder" },
    { "list",    "list all models"              },
    { "count",   "count models"                 },
};
#define MODEL_ACTIONS (sizeof(model_actions) / sizeof(model_actions[0]))

int cmd_model(const char *action, cmd_args_t *ga, const global_opts_t *gopts,
              db_t *db)
{
    (void)ga; (void)gopts; (void)db;

    if (strcmp(action, "create")  == 0) { fprintf(stderr, "[model.create] stub\n");  return EXIT_OK; }
    if (strcmp(action, "get")     == 0) { fprintf(stderr, "[model.get] stub\n");     return EXIT_OK; }
    if (strcmp(action, "update")  == 0) { fprintf(stderr, "[model.update] stub\n");  return EXIT_OK; }
    if (strcmp(action, "delete")  == 0) { fprintf(stderr, "[model.delete] stub\n");  return EXIT_OK; }
    if (strcmp(action, "restore") == 0) { fprintf(stderr, "[model.restore] stub\n"); return EXIT_OK; }
    if (strcmp(action, "move")    == 0) { fprintf(stderr, "[model.move] stub\n");    return EXIT_OK; }
    if (strcmp(action, "list")    == 0) { fprintf(stderr, "[model.list] stub\n");    return EXIT_OK; }
    if (strcmp(action, "count")   == 0) { fprintf(stderr, "[model.count] stub\n");   return EXIT_OK; }

    return action_err("model", action, model_actions, MODEL_ACTIONS);
}
