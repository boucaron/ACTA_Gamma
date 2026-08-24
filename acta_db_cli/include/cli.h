#ifndef ACTA_CLI_H
#define ACTA_CLI_H

#include <stdio.h>
#include <stdlib.h>

/* ---- version ---- */
#define ACTA_DB_CLI_VERSION "0.1.0"

/* ---- exit codes (mirrors spec §7.1) ---- */
#define EXIT_OK             0
#define EXIT_NOT_FOUND      1
#define EXIT_SQL            2
#define EXIT_ALLOC          3
#define EXIT_INVALID        4
#define EXIT_CLI            10
#define EXIT_DB_OPEN        11

/* ---- global options (filled by parse_globals) ---- */
typedef struct {
    const char *db;          /* --db */
    const char *fields;      /* --fields */
    int         no_nulls;    /* --no-nulls */
    int         id_only;     /* --id-only */
    int         count;       /* --count */
    int         table;       /* --table */
    int         pretty;      /* --pretty */
    const char *json_input;  /* --json */
    int         from_stdin;  /* --stdin */
    const char *from_file;   /* --from-file */
    int         show_version;/* --version */
    int         show_help;   /* --help */
    int         show_tools;  /* --tools */

    /* remaining argv after global extraction */
    int   argc;
    char **argv;
} global_opts_t;

/* ---- resolved DB path ---- */
const char *resolve_db_path(const char *flag_db);

/* ---- helpers ---- */
void cli_error(int exit_code, const char *err_const, int c_code, const char *fmt, ...);

#endif /* ACTA_CLI_H */
