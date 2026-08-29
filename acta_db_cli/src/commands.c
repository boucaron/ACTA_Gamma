#include "commands.h"
#include "argparse.h"
#include "cli.h"

#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

/* Single definition; set once in commands_dispatch, read by VLOG()
 * (cli.h) from every translation unit. */
const global_opts_t *cli_gopts = NULL;

#ifndef ACTA_DB_GIT_HASH
#define ACTA_DB_GIT_HASH "unknown"
#endif

/* ------------------------------------------------------------------ */
/*  entity dispatch table                                              */
/* ------------------------------------------------------------------ */

typedef int (*entity_fn)(const char *action, cmd_args_t *ga,
                         const global_opts_t *gopts, db_t *db);

typedef struct {
    const char *name;
    entity_fn   fn;
} entity_entry_t;

static const entity_entry_t entity_table[] = {
    { "db",                 cmd_db           },
    { "context",            cmd_context      },
    { "model",              cmd_model        },
    { "model_folder",       cmd_model_folder },
    { "model_revision",     cmd_model_revision    },
    { "skill",              cmd_skill        },
    { "skill_folder",       cmd_skill_folder },
    { "skill_revision",     cmd_skill_rev    },
    { "exec",               cmd_exec         },
    { "log",                cmd_execution_log},
};

#define ENTITY_COUNT (sizeof(entity_table) / sizeof(entity_table[0]))

static entity_fn lookup_entity(const char *entity) {
    for (size_t i = 0; i < ENTITY_COUNT; ++i) {
        if (strcmp(entity_table[i].name, entity) == 0)
            return entity_table[i].fn;
    }
    return NULL;
}

static int entity_not_found(const char *entity) {
    fprintf(stderr,
        "{\"error\":\"ACTA_CLI_ERR\",\"code\":-10,"
        "\"message\":\"unknown entity: %s\"}\n",
        entity);
    return EXIT_CLI;
}

/* ------------------------------------------------------------------ */
/*  public dispatch                                                    */
/* ------------------------------------------------------------------ */

int commands_dispatch(const char *entity, const char *action,
                      cmd_args_t *ga, const global_opts_t *gopts,
                      db_t *db)
{
    (void)action; /* entity handlers parse action themselves for now */

    entity_fn fn = lookup_entity(entity);
    if (!fn)
        return entity_not_found(entity);

    cli_gopts = gopts;   /* ← makes VLOG() see the current verbose level */
    VLOG(2, "dispatch to %s", entity);
    return fn(action, ga, gopts, db);
}

/* ------------------------------------------------------------------ */
/*  --version / --help / --tools                                       */
/* ------------------------------------------------------------------ */

void version_print(FILE *out)
{
    fprintf(out, "actagamma_db %s (%s | libacta_db, sqlite %s)\n",
            ACTA_DB_CLI_VERSION, ACTA_DB_GIT_HASH, sqlite3_libversion());
}

void help_print(FILE *out)
{
    fprintf(out,
        "usage: actagamma_db [global-flags] <entity> <action> [args]\n"
        "\n"
        "entities: db, context, model, model_folder, model_revision,\n"
        "          skill, skill_folder, skill_revision, exec, log\n"
        "\n"
        "global flags:\n"
        "  --db <path>        database file\n"
        "  --fields <f1,f2>   output field filter\n"
        "  --no_nulls         omit null fields\n"
        "  --id_only          print only the id\n"
        "  --count            print only the row count\n"
        "  --table            columnar output\n"
        "  --pretty           2-space indent JSON\n"
        "  --json <blob>      input JSON object\n"
        "  --stdin            read JSON from stdin\n"
        "  --from_file <p>    read JSON from file\n"
        "  --version          print version\n"
        "  --help, -h         this help\n"
        "  --tools            JSON tool schema\n"
        "  --verbose, -v      increase verbosity (repeatable, 1-3)\n"
        "\n"
        "see --tools for full command reference.\n");
}

int tools_print(FILE *out)
{
    /* TODO: emit the full JSON array from spec §11 */
    fprintf(out, "[]");
    return EXIT_OK;
}
