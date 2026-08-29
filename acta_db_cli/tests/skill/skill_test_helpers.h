#ifndef SKILL_TEST_HELPERS_H
#define SKILL_TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "acta_db.h"
#include "argparse.h"
#include "cli_util.h"
#include "commands.h"

/* ── test context: one duplicated DB + captured stdout ───────────── */

typedef struct {
    char    db_path[512];       /* temp copy of acta_test_ref.db */
    db_t  *db;
    FILE  *out;                 /* redirected stdout */
    int saved_stdout; 
    char  *out_buf;             /* heap buffer holding captured stdout */
    size_t out_len;             /* bytes captured */
    size_t out_cap;
    int    failures;            /* incremented by TEST_* macros */
    int    assertions;          /* total assertions run */
} stest_ctx_t;

/* lifecycle */
void stest_init(stest_ctx_t *ctx, const char *ref_db_path);
void stest_teardown(stest_ctx_t *ctx);

/* stdout capture */
void stest_capture_begin(stest_ctx_t *ctx);
void stest_capture_end(stest_ctx_t *ctx);
const char *stest_stdout(stest_ctx_t *ctx);

/* assert helpers (non-fatal, count failures) */
void stest_assert(stest_ctx_t *ctx, int cond, const char *file, int line, const char *fmt, ...);
void stest_assert_int_eq(stest_ctx_t *ctx, int got, int want, const char *file, int line, const char *what);
void stest_assert_str_eq(stest_ctx_t *ctx, const char *got, const char *want, const char *file, int line, const char *what);
void stest_assert_null(stest_ctx_t *ctx, const void *p, const char *file, int line, const char *what);
void stest_assert_not_null(stest_ctx_t *ctx, const void *p, const char *file, int line, const char *what);
void stest_assert_contains(stest_ctx_t *ctx, const char *haystack, const char *needle, const char *file, int line, const char *what);

#define TARGS_MAX_TOK  64
/* ── test-args builder (wraps cmd_args_t construction) ───────────── */
cmd_args_t *targs_new(void);
void targs_flag(cmd_args_t *a, const char *name, const char *value, global_opts_t *opts);
void targs_flag_bool(cmd_args_t *a, const char *name, global_opts_t *opts);
void targs_pos(cmd_args_t *a, const char *value, global_opts_t *opts);
void targs_free(cmd_args_t *a, global_opts_t *opts);

/* ── global_opts convenience ──────────────────────────────────────── */
global_opts_t gopts_default(void);
global_opts_t gopts_json(void);       /* json_input = 1 */
global_opts_t gopts_id_only(void);
global_opts_t gopts_table(void);
global_opts_t gopts_fields(const char *csv);
global_opts_t gopts_no_nulls(void);

/* ── seed helpers (operate on the live test DB) ───────────────────── */
int stest_seed_skill(stest_ctx_t *ctx, int folder_id,
                     const char *name, const char *prompt,
                     const char *desc, const char *schema);
int stest_seed_folder(stest_ctx_t *ctx, const char *name, int parent_id);

/* ── the macros ───────────────────────────────────────────────────── */
#define TEST(ctx, cond) \
    stest_assert((ctx), (cond), __FILE__, __LINE__, "assert: %s", #cond)

#define TEST_EQ(ctx, got, want) \
    stest_assert_int_eq((ctx), (int)(got), (int)(want), __FILE__, __LINE__, #got " == " #want)

#define TEST_STREQ(ctx, got, want) \
    stest_assert_str_eq((ctx), (got), (want), __FILE__, __LINE__, #got " == " #want)

#define TEST_NULL(ctx, p) \
    stest_assert_null((ctx), (p), __FILE__, __LINE__, #p " == NULL")

#define TEST_NOT_NULL(ctx, p) \
    stest_assert_not_null((ctx), (p), __FILE__, __LINE__, #p " != NULL")

#define TEST_CONTAINS(ctx, hay, needle) \
    stest_assert_contains((ctx), (hay), (needle), __FILE__, __LINE__, "stdout contains " #needle)

/* ── per-file runners (each .c exposes one) ──────────────────────── */
int run_skill_test_create(void);
int run_skill_test_get(void);
int run_skill_test_update(void);
int run_skill_test_delete_restore(void);
int run_skill_test_move(void);
int run_skill_test_list_count(void);
int run_skill_test_misc(void);

#endif /* SKILL_TEST_HELPERS_H */
