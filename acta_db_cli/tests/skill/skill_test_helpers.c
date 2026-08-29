#include "skill_test_helpers.h"
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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

cmd_args_t *targs_new()
{
    targs_internal_t *t = (targs_internal_t *)calloc(1, sizeof(targs_internal_t));
    if (!t) return NULL;
    t->pos_idx = 0;
    t->owned = 1;
    /* ADAPT: call whatever cmd_args_t needs to be in a "ready for
     * cmd_args_flag / cmd_args_next_positional" state.
     * e.g. cmd_args_init(&t->raw);  or  t->raw = (cmd_args_t){0};
     */

    t->raw.argc = 0;
    t->raw.argv = malloc(sizeof(char *) * TARGS_MAX_TOK);

    return &t->raw;
}

void targs_flag(cmd_args_t *a, const char *name, const char *value, global_opts_t *opts)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT: recover internal ptr */
    if (t->n_flags >= MAX_FLAGS) return;
    snprintf(t->names[t->n_flags], MAX_LEN, "%s", name);
    snprintf(t->values[t->n_flags], MAX_LEN, "%s", value ? value : "");
    t->has_value[t->n_flags] = 1;
    t->n_flags++;
    /* ADAPT: store into cmd_args_t internal representation */


    char buf[64];
    snprintf(buf, sizeof buf, "--%s", name);
    opts->argv[opts->argc++] = strdup(buf);
    if (value)
        opts->argv[opts->argc++] = strdup(value);    
    a->argv[a->argc] = strdup(buf);
    a->argc++;
    a->argv[a->argc] = strdup(value);
    a->argc++;


}

void targs_flag_bool(cmd_args_t *a, const char *name, global_opts_t *opts)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT */
    if (t->n_flags >= MAX_FLAGS) return;
    snprintf(t->names[t->n_flags], MAX_LEN, "%s", name);
    t->values[t->n_flags][0] = '\0';
    t->has_value[t->n_flags] = 0;
    t->n_flags++;
    /* ADAPT */

    char buf[64];
    snprintf(buf, sizeof buf, "--%s", name);
    opts->argv[opts->argc++] = strdup(buf);
   //  opts->argv[opts->argc++] = strdup(value);
    a->argv[a->argc] = strdup(buf);
    a->argc++;

}

void targs_pos(cmd_args_t *a, const char *value, global_opts_t *opts)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT */
    if (t->n_pos >= MAX_POS) return;
    snprintf(t->pos[t->n_pos], MAX_LEN, "%s", value);
    t->n_pos++;
    /* ADAPT */

    opts->argv[opts->argc++] = strdup(value);

    a->argv[a->argc] = strdup(value);
    a->argc++;
}

void targs_free(cmd_args_t *a, global_opts_t *opts)
{
    targs_internal_t *t = (targs_internal_t *)a;  /* ADAPT */
    if (t && t->owned) free(t);

    // memleak in opts... not a big deal
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
    g.argc = 0;
    g.argv = malloc(sizeof(char *) * TARGS_MAX_TOK);
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
    ctx->db = acta_db_open(ctx->db_path, &err, ACTA_DB_OPEN_EXISTING);
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
    if (ctx->input_path[0]) {
        unlink(ctx->input_path);
        ctx->input_path[0] = '\0';
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  stdout capture via dup2
 * ══════════════════════════════════════════════════════════════════ */

void stest_capture_begin(stest_ctx_t *ctx)
{
    fflush(stdout);

    int p[2];
#ifdef _WIN32
    _pipe(p, 65536, _O_BINARY); // MINGW
#else
    pipe(p);
#endif

    ctx->saved_stdout = dup(STDOUT_FILENO);
    dup2(p[1], STDOUT_FILENO);   /* ← the redirect */
    close(p[1]);                 /* only fd 1 holds the write end now */

    ctx->out = fdopen(p[0], "rb");  /* read end, for the slurp below */

    free(ctx->out_buf);
    ctx->out_buf  = NULL;
    ctx->out_len  = 0;
    ctx->out_cap  = 0;
}

void stest_capture_end(stest_ctx_t *ctx)
{
    if (ctx->saved_stdout < 0) return;

    fflush(stdout);                    /* drain stdio → pipe */
    dup2(ctx->saved_stdout, STDOUT_FILENO);  /* restore   */
    close(ctx->saved_stdout);
    ctx->saved_stdout = -1;

    /* slurp */
    free(ctx->out_buf);
    ctx->out_buf  = NULL;
    ctx->out_len  = 0;
    ctx->out_cap  = 0;

    if (ctx->out) {
        for (;;) {
            if (ctx->out_len >= ctx->out_cap) {
                ctx->out_cap = ctx->out_cap ? ctx->out_cap * 2 : 4096;
                ctx->out_buf = realloc(ctx->out_buf, ctx->out_cap);
                if (!ctx->out_buf) break;
            }
            size_t n = fread(ctx->out_buf + ctx->out_len, 1,
                             ctx->out_cap - ctx->out_len, ctx->out);
            if (n == 0) break;
            ctx->out_len += n;
        }
        ctx->out_buf[ctx->out_len] = '\0';
        fclose(ctx->out);
        ctx->out = NULL;
    }
}


const char *stest_stdout(stest_ctx_t *ctx)
{
    return ctx->out_buf ? ctx->out_buf : "";
}

/* ══════════════════════════════════════════════════════════════════
 *  argv-level runner (parse_globals + handler, like main.c)
 * ══════════════════════════════════════════════════════════════════ */

int stest_run_argv(stest_ctx_t *ctx, stest_cmd_fn_t fn,
                   int argc, char **argv, const char *stdin_blob)
{
    global_opts_t g;
    if (parse_globals(argc, argv, &g) != 0)
        return EXIT_CLI;

#ifdef _WIN32
    int saved_in  = _dup(STDIN_FILENO);
    int saved_out = _dup(STDOUT_FILENO);
    int in_p[2], out_p[2];
    if (saved_in < 0 || saved_out < 0 ||
        _pipe(in_p,  65536, _O_BINARY) != 0 ||
        _pipe(out_p, 65536, _O_BINARY) != 0) {
        if (saved_in  >= 0) _close(saved_in);
        if (saved_out >= 0) _close(saved_out);
        free(g.argv);
        return EXIT_CLI;
    }
#else
    int saved_in  = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int in_p[2], out_p[2];
    if (saved_in < 0 || saved_out < 0 ||
        pipe(in_p) != 0 || pipe(out_p) != 0) {
        if (saved_in  >= 0) close(saved_in);
        if (saved_out >= 0) close(saved_out);
        free(g.argv);
        return EXIT_CLI;
    }
#endif

    /* feed stdin_blob to the handler; "" → immediate EOF */
    const char *data = stdin_blob ? stdin_blob : "";
    size_t blen = strlen(data);
    size_t off = 0;
    while (off < blen) {
        ssize_t w = write(in_p[1], data + off, blen - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    close(in_p[1]);

    dup2(in_p[0], STDIN_FILENO);
    dup2(out_p[1], STDOUT_FILENO);
    close(in_p[0]);
    close(out_p[1]);

    cmd_args_t ga;
    cmd_args_init(&ga, g.argc - 2, g.argv + 2);
    int rc = fn(g.argv[1], &ga, &g, ctx->db);

    fflush(stdout);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_in, STDIN_FILENO);
    close(saved_in);
    close(saved_out);

    /* slurp captured stdout → stest_stdout(ctx) */
    free(ctx->out_buf);
    ctx->out_buf  = NULL;
    ctx->out_len  = 0;
    ctx->out_cap  = 0;
    for (;;) {
        if (ctx->out_len >= ctx->out_cap) {
            ctx->out_cap = ctx->out_cap ? ctx->out_cap * 2 : 4096;
            ctx->out_buf = realloc(ctx->out_buf, ctx->out_cap);
            if (!ctx->out_buf) break;
        }
        ssize_t n = read(out_p[0], ctx->out_buf + ctx->out_len,
                         ctx->out_cap - ctx->out_len - 1);
        if (n <= 0) break;
        ctx->out_len += (size_t)n;
    }
    close(out_p[0]);
    if (ctx->out_buf) ctx->out_buf[ctx->out_len] = '\0';

    free(g.argv);
    return rc;
}

const char *stest_write_input(stest_ctx_t *ctx, const char *blob)
{
    static int counter = 0;
    snprintf(ctx->input_path, sizeof ctx->input_path,
             "./tmp/acta_test_in_%d_%d.json", (int)getpid(), counter++);
    FILE *fp = fopen(ctx->input_path, "wb");
    if (!fp) return NULL;
    if (blob) fputs(blob, fp);
    fclose(fp);
    return ctx->input_path;
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
