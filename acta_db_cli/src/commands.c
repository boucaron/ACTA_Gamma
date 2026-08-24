#include "commands.h"
#include "argparse.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  local dispatch per entity                                         */
/* ------------------------------------------------------------------ */

static int dispatch_entity(const char *entity, const char *action,
                           cmd_args_t *ga, const global_opts_t *gopts) {
    /*
     * Each entity handler is a simple if/else on `action`.
     * For the POC we just route to the stub.
     */
    if (strcmp(entity, "db") == 0)           return cmd_db(action, ga, gopts);
    if (strcmp(entity, "context") == 0)     return cmd_context(action, ga, gopts);
    if (strcmp(entity, "model") == 0)       return cmd_model(action, ga, gopts);
    if (strcmp(entity, "model-folder") == 0)return cmd_model_folder(action, ga, gopts);
    if (strcmp(entity, "model-rev") == 0)   return cmd_model_rev(action, ga, gopts);
    if (strcmp(entity, "skill") == 0)       return cmd_skill(action, ga, gopts);
    if (strcmp(entity, "skill-folder") == 0)return cmd_skill_folder(action, ga, gopts);
    if (strcmp(entity, "skill-rev") == 0)   return cmd_skill_rev(action, ga, gopts);
    if (strcmp(entity, "exec") == 0)        return cmd_exec(action, ga, gopts);
    if (strcmp(entity, "log") == 0)         return cmd_log(action, ga, gopts);

    /* unknown entity */
    fprintf(stderr, "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,\"message\":\"unknown entity: %s\"}\n", entity);
    return EXIT_CLI;
}

/* ------------------------------------------------------------------ */
/*  public dispatch                                                    */
/* ------------------------------------------------------------------ */

int commands_dispatch(const char *entity, const char *action,
                      cmd_args_t *ga, const global_opts_t *gopts) {
    (void)action; /* entity handlers parse action themselves for now */
    return dispatch_entity(entity, action, ga, gopts);
}

/* ------------------------------------------------------------------ */
/*  --version / --help / --tools                                      */
/* ------------------------------------------------------------------ */

void version_print(FILE *out) {
    fprintf(out, "actagamma_db %s (libacta_db 0.1.0, sqlite 3.x.x)\n",
            ACTA_DB_CLI_VERSION);
}

void help_print(FILE *out) {
    fprintf(out,
        "usage: actagamma_db [global-flags] <entity> <action> [args]\n"
        "\n"
        "entities: db, context, model, model-folder, model-rev,\n"
        "          skill, skill-folder, skill-rev, exec, log\n"
        "\n"
        "global flags:\n"
        "  --db <path>        database file\n"
        "  --fields <f1,f2>   output field filter\n"
        "  --no-nulls         omit null fields\n"
        "  --id-only          print only the id\n"
        "  --count            print only the row count\n"
        "  --table            columnar output\n"
        "  --pretty           2-space indent JSON\n"
        "  --json <blob>      input JSON object\n"
        "  --stdin            read JSON from stdin\n"
        "  --from-file <p>    read JSON from file\n"
        "  --version          print version\n"
        "  --help, -h         this help\n"
        "  --tools            JSON tool schema\n"
        "\n"
        "see --tools for full command reference.\n");
}

int tools_print(FILE *out) {
    /* TODO: emit the full JSON array from spec §11 */
    fprintf(out, "[]");
    return EXIT_OK;
}
