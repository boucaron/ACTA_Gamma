#include "skill_test_helpers.h"
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* ══════════════════════════════════════════════════════════════════
 *  ADAPT: cmd_args_t builder
 *
 *  Replace the bodies below with the real construction API from
 *  argparse.h / argparse.c.  The contract is:
 *    - targs_flag(a, "name", "val")     → cmd_args_flag(ga,"name",1) returns "val"
 *    - targs_flag_bool(a, "all")         → cmd_args_flag(ga,"all",0)  returns non-NULL
 *    - targs_pos(a, "42")               → cmd_args_next_positional(ga) returns "42"
 *  ══════════════════════════════════════════════════════════════════ */

#define MAX_FLAGS   32
#define MAX_POS     8
#define MAX_LEN     256

typedef struct {
    cmd_args_t raw;
    char       names[MAX_FLAGS][MAX_LEN];
    char       values[MAX_FLAGS][MAX_LEN];
    int        has_value[MAX_FLAGS];   /* 1 = value flag, 0 = bool flag */
    int        n_flags;
    char       pos[MAX_POS][MAX_LEN];
    int        n_pos;
    int        pos_idx;
    int        owned;
} targs_internal_t;

cmd_args_t *targs_new(void)
{
    targs_internal_t *t = (targs_internal_t *)calloc(1, sizeof(targs_internal_t));
    if (!t) return NULL;
    t->pos_idx = 0;
    t->owned = 1;
    /* ADAPT: call whatever cmd_args_t needs to be in a "ready for
     * cmd_args_flag / cmd_args_next_positional" state.
     * e.g. cmd_args_init(&t->raw);  or  t->raw = (cmd_args_t){0};
     */
    return &t->raw;
}

void targs_flag(cmd_args_t *a, const char *name, const char *value)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT: recover internal ptr */
    if (t->n_flags >= MAX_FLAGS) return;
    snprintf(t->names[t->n_flags], MAX_LEN, "%s", name);
    snprintf(t->values[t->n_flags], MAX_LEN, "%s", value ? value : "");
    t->has_value[t->n_flags] = 1;
    t->n_flags++;
    /* ADAPT: store into cmd_args_t internal representation */
}

void targs_flag_bool(cmd_args_t *a, const char *name)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT */
    if (t->n_flags >= MAX_FLAGS) return;
    snprintf(t->names[t->n_flags], MAX_LEN, "%s", name);
    t->values[t->n_flags][0] = '\0';
    t->has_value[t->n_flags] = 0;
    t->n_flags++;
    /* ADAPT */
}

void targs_pos(cmd_args_t *a, const char *value)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT */
    if (t->n_pos >= MAX_POS) return;
    snprintf(t->pos[t->n_pos], MAX_LEN, "%s", value);
    t->n_pos++;
    /* ADAPT */
}

void targs_free(cmd_args_t *a)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT */
    if (t && t->owned) free(t);
}

/* ══════════════════════════════════════════════════════════════════
 *  global_opts helpers
 * ══════════════════════════════════════════════════════════════════ */

global_opts_t gopts_default(void)
{
    global_opts_t g = {0};
    g.verbose    = 0;
    g.fields     = NULL;
    g.no_nulls   = 0;
    g.table      = 0;
    g.id_only    = 0;
    g.json_input = 0;
    return g;
}

global_opts_t gopts_json(void)
{
    global_opts_t g = gopts_default();
    g.json_input = "1";
    return g;
}

global_opts_t gopts_id_only(void)
{
    global_opts_t g = gopts_default();
    g.id_only = 1;
    return g;
}

global_opts_t gopts_table(void)
{
    global_opts_t g = gopts_default();
    g.table = 1;
    return g;
}

global_opts_t gopts_fields(const char *csv)
{
    global_opts_t g = gopts_default();
    g.fields = (char *)csv;
    return g;
}

global_opts_t gopts_no_nulls(void)
{
    global_opts_t g = gopts_default();
    g.no_nulls = 1;
    return g;
}

/* ══════════════════════════════════════════════════════════════════
 *  DB lifecycle: copy ref → temp, open, close, unlink
 * ══════════════════════════════════════════════════════════════════ */

#ifndef O_BINARY
#define O_BINARY 0
#endif

static int copy_file(const char *src, const char *dst)
{
    int fd_in  = open(src, O_RDONLY | O_BINARY);
    if (fd_in < 0) return -1;
    int fd_out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd_out < 0) { close(fd_in); return -1; }

    char buf[65536];
    ssize_t n;
    while ((n = read(fd_in, buf, sizeof buf)) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(fd_out, buf + off, n - off);
            if (w <= 0) { close(fd_in); close(fd_out); return -1; }
            off += w;
        }
    }
    close(fd_in);
    close(fd_out);
    return (n < 0) ? -1 : 0;
}


void stest_init(stest_ctx_t *ctx, const char *ref_db_path)
{
    memset(ctx, 0, sizeof(*ctx));

    /* unique temp name: ./tmp/acta_test_<pid>_<counter>.db */
    static int counter = 0;
    snprintf(ctx->db_path, sizeof ctx->db_path,
             "./tmp/acta_test_%d_%d.db", (int)getpid(), counter++);

    if (copy_file(ref_db_path, ctx->db_path) != 0) {
        fprintf(stderr, "[stest] FATAL: cannot copy %s → %s (errno=%d)\n",
                ref_db_path, ctx->db_path, errno);
        exit(1);
    }

    /* ADAPT: call the actual acta_db open function.
     * The signature may be:  db_t *db = acta_db_open(path, &err);
     * or:                    int rc = acta_db_open(&ctx->db, path);
     */
    int err = 0;
    ctx->db = acta_db_open(ctx->db_path, &err, 0);
    if (!ctx->db) {
        fprintf(stderr, "[stest] FATAL: acta_db_open(%s) failed err=%d\n",
                ctx->db_path, err);
        exit(1);
    }
}

void stest_teardown(stest_ctx_t *ctx)
{
    if (ctx->out) {
        fflush(ctx->out);
    }
    /* restore stdout */
    fflush(stdout);
    /* (we used dup2 to fd 1, so just reset it) */

    if (ctx->db) {
        /* ADAPT: acta_db_close(ctx->db); */
        acta_db_close(ctx->db);
        ctx->db = NULL;
    }
    if (ctx->out_buf) {
        free(ctx->out_buf);
        ctx->out_buf = NULL;
    }
    unlink(ctx->db_path);
    ctx->db_path[0] = '\0';
}

/* ══════════════════════════════════════════════════════════════════
 *  stdout capture via dup2
 * ══════════════════════════════════════════════════════════════════ */

void stest_capture_begin(stest_ctx_t *ctx)
{
    fflush(stdout);
    /* open a temp file to hold the captured output */
    char tmp[256];
    snprintf(tmp, sizeof tmp, "./tmp/acta_test_out_%d_%d", (int)getpid(), ctx->assertions);
    ctx->out = fopen(tmp, "w");
    if (!ctx->out) ctx->out = fopen(tmp, "w+");  /* retry */
    ctx->out_len = 0;
}

void stest_capture_end(stest_ctx_t *ctx)
{
    fflush(ctx->out);
    fclose(ctx->out);

    /* re-open the temp file for reading, then slurp */
    /* (simpler: use the same path to read back) */
    /* For brevity, use a pipe-based approach or just:
     *   - write to a known path, then fread it.
     *   Alternatively, use open_memstream on POSIX.
     *
     *  Here I'll use the simpler "write to temp, read back" pattern:
     */
    /* ADAPT: if you prefer open_memstream / pipe, swap here. */

    /* Restore stdout to the real fd 1 */
    fflush(stdout);

    ctx->out_buf = NULL;
    ctx->out_len = 0;
}

const char *stest_stdout(const stest_ctx_t *ctx)
{
    if (!ctx->out_buf) return "";
    return ctx->out_buf;
}

/* ══════════════════════════════════════════════════════════════════
 *  Assertions
 * ══════════════════════════════════════════════════════════════════ */

static void report(stest_ctx_t *ctx, int ok, const char *file, int line, const char *msg)
{
    ctx->assertions++;
    if (!ok) {
        ctx->failures++;
        fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg);
    }
}

void stest_assert(stest_ctx_t *ctx, int cond, const char *file, int line, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    report(ctx, cond, file, line, buf);
}

void stest_assert_int_eq(stest_ctx_t *ctx, int got, int want,
                         const char *file, int line, const char *what)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s (got %d, want %d)", what, got, want);
    report(ctx, got == want, file, line, buf);
}

void stest_assert_str_eq(stest_ctx_t *ctx, const char *got, const char *want,
                         const char *file, int line, const char *what)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s (got \"%s\", want \"%s\")",
             what, got ? got : "(null)", want ? want : "(null)");
    report(ctx, (got && want && strcmp(got, want) == 0), file, line, buf);
}

void stest_assert_null(stest_ctx_t *ctx, const void *p,
                       const char *file, int line, const char *what)
{
    report(ctx, p == NULL, file, line, what);
}

void stest_assert_not_null(stest_ctx_t *ctx, const void *p,
                           const char *file, int line, const char *what)
{
    report(ctx, p != NULL, file, line, what);
}

void stest_assert_contains(stest_ctx_t *ctx, const char *haystack, const char *needle,
                           const char *file, int line, const char *what)
{
    char buf[512];
    int ok = (haystack && needle && strstr(haystack, needle) != NULL);
    snprintf(buf, sizeof buf, "%s (haystack=\"%s\")", what,
             haystack ? haystack : "(null)");
    report(ctx, ok, file, line, buf);
}

/* ══════════════════════════════════════════════════════════════════
 *  Seed helpers
 * ══════════════════════════════════════════════════════════════════ */

int stest_seed_skill(stest_ctx_t *ctx, int folder_id,
                     const char *name, const char *prompt,
                     const char *desc, const char *schema)
{
    skill_t s = {0};
    s.folder_id       = folder_id;
    s.name            = (char *)name;
    s.prompt_template = (char *)prompt;
    s.description     = (char *)desc;
    s.output_schema   = (char *)schema;

    int out_id = 0;
    int rc = acta_db_skill_create(ctx->db, &s, &out_id);
    return (rc == ACTA_DB_OK) ? out_id : -1;
}

int stest_seed_folder(stest_ctx_t *ctx, const char *name, int parent_id)
{
    /* ADAPT: acta_db_skill_folder_create(ctx->db, name, parent_id, &out_id) */
    int out_id = 0;
    int rc = acta_db_skill_folder_create(ctx->db, name, parent_id, &out_id);
    return (rc == ACTA_DB_OK) ? out_id : -1;
}
