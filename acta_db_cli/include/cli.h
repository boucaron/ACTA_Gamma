#ifndef ACTA_DB_CLI_H
#define ACTA_DB_CLI_H

#include <stdio.h>
#include <stdarg.h>
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
    /* input / source */
    const char *db;          /* --db */
    const char *fields;      /* --fields */
    const char *json_input;  /* --json */
    int         from_stdin;  /* --stdin */
    const char *from_file;   /* --from_file */

    /* output shaping */
    int         no_nulls;    /* --no_nulls */
    int         id_only;     /* --id_only */
    int         count;       /* --count */
    int         table;       /* --table */
    int         pretty;      /* --pretty */

    /* meta */
    int         show_version;/* --version */
    int         show_help;   /* --help */
    int         show_tools;  /* --tools */

    /* diagnostics */
    int         verbose;     /* -v / --verbose (0–3) */

    /* remaining argv after global extraction */
    int   argc;
    char **argv;
} global_opts_t;

/* ---- verbose helpers ---- */

/*
 * vdbg(GO, LEVEL, "fmt", ...)
 *   Prints to stderr only when GO->verbose >= LEVEL.
 *   LEVEL 1 = info,  2 = debug,  3 = trace.
 *
 *   GO is a global_opts_t * (may be NULL → no-op).
 */
#define vdbg(GO, LEVEL, ...) do { \
    if ((GO) && (GO)->verbose >= (LEVEL)) { \
        fprintf(stderr, "[v" #LEVEL "] "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)



/* ---- resolved DB path ---- */
const char *resolve_db_path(const char *flag_db);

/* ---- helpers ---- */
void cli_error(int exit_code, const char *err_const, int c_code, const char *fmt, ...);

#endif /* ACTA_CLI_H */
