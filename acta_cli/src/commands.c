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
typedef int (*action_help_fn)(const char *action, FILE *out);
typedef void (*full_help_fn)(FILE *out);

typedef struct {
    const char *name;
    entity_fn      fn;
    full_help_fn   full_help;      /* P0: whole-entity help printer */
    action_help_fn help_for_action; /* P0: single-action help printer */
} entity_entry_t;

static const entity_entry_t entity_table[] = {
    { "db",                 cmd_db,             db_usage,             db_help_for_action          },
    { "context",            cmd_context,        ctx_usage,            context_help_for_action     },
    { "model",              cmd_model,          model_usage,          model_help_for_action       },
    { "model_folder",       cmd_model_folder,   model_folder_usage,   model_folder_help_for_action},
    { "model_revision",     cmd_model_revision, model_revision_usage, model_revision_help_for_action},
    { "skill",              cmd_skill,          skill_usage,          skill_help_for_action       },
    { "skill_folder",       cmd_skill_folder,   skill_folder_usage,   skill_folder_help_for_action},
    { "skill_revision",     cmd_skill_rev,      skill_rev_usage,      skill_revision_help_for_action},
    { "exec",               cmd_exec,           exec_usage,           exec_help_for_action        },
    { "log",                cmd_execution_log,  execution_log_usage,  log_help_for_action         },
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
    char msg[128];
    snprintf(msg, sizeof msg, "unknown entity: %s",
             entity ? entity : "(null)");
    int rc = emit_cli_error(msg);   /* JSON contract line, stderr line 1 */

    /* Suggestion line, appended *after* the JSON contract line (scripts
     * parse line 1), same fuzzy matcher as unknown_action(). */
    const char *names[ENTITY_COUNT];
    for (size_t i = 0; i < ENTITY_COUNT; ++i)
        names[i] = entity_table[i].name;
    const char *guess = closest_name(entity, names, ENTITY_COUNT);
    if (guess)
        fprintf(stderr, "  Did you mean '%s'?\n", guess);
    return rc;
}

/* ------------------------------------------------------------------ */
/*  P0: entity/action help router                                       */
/* ------------------------------------------------------------------ */

/*
 * Shared help printer for main.c's `--help` routing:
 *   - action == NULL or "help"  → whole entity help
 *   - action == known action    → that action's section only
 *   - unknown action            → canonical JSON error, EXIT_CLI
 *   - unknown entity            → canonical JSON error, EXIT_CLI
 * No DB handle needed. The per-file `help` branch (X help <action>)
 * funnels into the same X_help_for_action lookups, so both entry points
 * cannot drift.
 */
int entity_help(const char *entity, const char *action, FILE *out)
{
    for (size_t i = 0; i < ENTITY_COUNT; ++i) {
        if (strcmp(entity_table[i].name, entity) == 0) {
            if (!action || strcmp(action, "help") == 0) {
                entity_table[i].full_help(out);
                return EXIT_OK;
            }
            if (entity_table[i].help_for_action(action, out) == 0)
                return EXIT_OK;

            char msg[128];
            snprintf(msg, sizeof msg, "unknown action: %s", action);
            int rc = emit_cli_error(msg);
            fprintf(stderr,
                    "Unknown action '%s'.\n"
                    "  Run 'acta_cli %s help' for full usage.\n",
                    action, entity);
            return rc;
        }
    }
    return entity_not_found(entity);
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
    int rc = fn(action, ga, gopts, db);

    /* KI-4: unexpected positional args used to be silently swallowed
     * (`context list help` ran the list, exit 0). Handlers read their
     * positionals with cmd_args_next_positional, which advances ga->pos;
     * anything left unconsumed after a successful handler run is an
     * error, not data. */
    if (rc == EXIT_OK) {
        const char *extra = cmd_args_next_positional(ga);
        if (extra) {
            char msg[256];
            snprintf(msg, sizeof msg,
                     "unexpected argument: '%s'", extra);
            emit_cli_error(msg);   /* JSON contract line, stderr line 1 */
            fprintf(stderr, "  Run 'acta_cli %s help' for usage.\n", entity);
            return EXIT_CLI;
        }
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/*  --version / --help / --tools                                       */
/* ------------------------------------------------------------------ */

void version_print(FILE *out)
{
    fprintf(out, "acta_cli %s (%s | libacta_db, sqlite %s)\n",
            ACTA_DB_CLI_VERSION, ACTA_DB_GIT_HASH, sqlite3_libversion());
}

void help_print(FILE *out)
{
    fprintf(out,
        "usage: acta_cli [global-flags] <entity> <action> [args]\n"
        "\n"
        "entities: db, context, model, model_folder, model_revision,\n"
        "          skill, skill_folder, skill_revision, exec, log\n"
        "\n"
        "global flags:\n"
        "  --db <path>        database file (default: $ACTA_DB,\n"
        "                     else ./acta.db)\n"
        "  --fields <f1,f2>   output field filter\n"
        "  --no_nulls         omit null fields\n"
        "  --id_only          print only the id\n"
        "  --count            print only the row count\n"
        "  --table            columnar output\n"
        "  --stream           NDJSON: one JSON object per line (list "
        "actions; pages internally, P5)\n"
        "  --pretty           2-space indent JSON (applies to --tools\n"
        "                     output)\n"
        "  --json <blob>      input JSON object\n"
        "  --stdin            read JSON from stdin\n"
        "  --from_file <p>    read JSON from file\n"
        "                     (--json / --stdin / --from_file are mutually\n"
        "                     exclusive; combining two is an error)\n"
        "  --out <path>       write the stdout payload to <path> instead of\n"
        "                     stdout (errors/warnings stay on stderr;\n"
        "                     ignored with --version/--help/--tools)\n"
        "  --raw_out <field>  print one field's raw (unescaped) value —\n"
        "                     'context get' / 'exec get' only; takes\n"
        "                     precedence over --id_only/--table/--fields;\n"
        "                     null values produce no output\n"
        "  --version          print version\n"
        "  --help, -h         this help\n"
        "  --tools            JSON tool schema\n"
        "  --compact          compact schema (with --tools): one line\n"
        "                     per command\n"
        "  --verbose, -v      increase verbosity (repeatable, 1-3)\n"
        "\n"
        "see --tools for full command reference.\n");
}

/* tools_print() moved to src/tools.c (T3): the static per-(entity,
 * action) data table plus its JSON renderer live there. */
