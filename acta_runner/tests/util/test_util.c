/*
 * test_util.c — the emitter contract of include/runner_util.h.
 *
 * Every acta_runner diagnostic is ONE machine-parsable JSON line
 * (docs/runner_contract.md, "Error contract"):
 *
 *   DB failure      {"error":"ACTA_DB_ERR_<NAME>","code":<rc>,"message":…}
 *   runner failure  {"error":"ACTA_RUNNER_ERROR","code":<exit code>,"message":…}
 *   check verdict   {"ok":true|false,…}  (stdout)
 *
 * and the "code" field always equals the exit code the emitter returns.
 *
 * This suite captures at the fd level (dup + dup2 + a temp file), calls the
 * header-inline emitters, and asserts the captured bytes EXACTLY (one line,
 * nothing else) and that the line PARSES as JSON with the contracted keys.
 * Both assertions matter: a line with a broken quote is a contract break
 * even when the exit code is right. The suite was written for exactly such a
 * defect — emit_runner_error() printed
 *
 *   {error":"ACTA_RUNNER_ERROR","code":4,…}      (missing opening quote)
 *
 * because its format string was "{" + "error\"…" (two adjacent literals)
 * instead of "{\"error\"…". Scenario 1 pins the fixed shape; scenario 8
 * proves this harness actually rejects the broken line.
 *
 * No HTTP, no DB, no stub server: the emitters are header-inline.
 * Exit code: 0 = all pass, 1 = at least one failure.
 */

#include "runner_util.h"

#ifdef _WIN32
#include <io.h>          /* open / close / dup / dup2 */
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

/* main.c owns runner_gopts in the real binary; this suite links no main.c,
 * so it owns the symbol here (VLOG is not used, but the extern must resolve
 * for any user of runner.h). */
const global_opts_t *runner_gopts;

static int checks;
static int failures;

static void check(int cond, const char *what)
{
    checks++;
    if (cond) {
        printf("  PASS %s\n", what);
    } else {
        printf("  FAIL %s\n", what);
        failures++;
    }
}

static void check_int(int got, int want, const char *what)
{
    checks++;
    if (got == want) {
        printf("  PASS %s\n", what);
    } else {
        printf("  FAIL %s (got %d, want %d)\n", what, got, want);
        failures++;
    }
}

/* ── fd-level capture ─────────────────────────────────────────────── */

/* The capture file lives in the scratch dir resolved in main(). */
static char capfile[512];

/* One emitter call, described declaratively so a single callback can drive
 * every scenario inside the capture window. */
enum {
    CALL_EMIT_RUNNER_ERROR = 1,  /* a = exit code,    s1 = message */
    CALL_FINISH_DB_ERROR,        /* a = rc,           s1 = what    */
    CALL_EMIT_ERROR,             /*                   s1 = message */
    CALL_EMIT_NOT_FOUND,         /*                   s1 = entity  */
    CALL_FINISH_OP_ERROR,        /* a = rc,           s1 = op      */
    CALL_VERDICT_OK,             /* a = max_context,  s1 = model, s2 = base_url */
    CALL_VERDICT_FAIL            /*                   s1 = model, s2 = base_url, s3 = verdict */
};

typedef struct {
    int         kind;
    int         a;
    const char *s1;
    const char *s2;
    const char *s3;
    int         rc_out;          /* out: what the emitter returned */
} emit_call_t;

static void emit_call(void *arg)
{
    emit_call_t *c = (emit_call_t *)arg;

    switch (c->kind) {
    case CALL_EMIT_RUNNER_ERROR:
        c->rc_out = emit_runner_error(c->a, c->s1);
        break;
    case CALL_FINISH_DB_ERROR:
        c->rc_out = finish_db_error(c->a, c->s1);
        break;
    case CALL_EMIT_ERROR:
        emit_error(c->s1);
        c->rc_out = 0;
        break;
    case CALL_EMIT_NOT_FOUND:
        c->rc_out = emit_not_found(c->s1);
        break;
    case CALL_FINISH_OP_ERROR:
        /* NULL db: the detail is acta_db_last_error(NULL) = no detail,
         * which is the documented NULL-safe path. */
        c->rc_out = finish_op_error(NULL, c->a, c->s1);
        break;
    case CALL_VERDICT_OK:
        emit_check_verdict(1, c->s1, c->s2, (long)c->a, NULL);
        c->rc_out = 0;
        break;
    case CALL_VERDICT_FAIL:
        emit_check_verdict(0, c->s1, c->s2, 0, c->s3);
        c->rc_out = 0;
        break;
    default:
        c->rc_out = -1;
        break;
    }
}

/* Redirect `fd` (STDOUT_FILENO or STDERR_FILENO) to capfile while fn(arg)
 * runs, restore the real fd, and return the captured bytes (malloc'd,
 * NUL-terminated, every \r removed so the comparison is platform-neutral:
 * Windows stdio writes \r\n). NULL on failure — the caller SKIPs and must
 * not assert.
 *
 * The FILE streams are never closed: only the fd is re-pointed, so the
 * suite keeps printing its own report after the capture. fflush(NULL) before
 * the redirect pushes any pending report line to the real destination, so it
 * cannot leak into the capture file. */
static char *capture(int fd, void (*fn)(void *), void *arg)
{
    fflush(NULL);

    remove(capfile);

    int saved = dup(fd);
    if (saved < 0)
        return NULL;

    int f = open(capfile, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (f < 0) {
        close(saved);
        return NULL;
    }
    dup2(f, fd);
    close(f);

    fn(arg);

    /* Flush the stream bound to fd so its bytes land in the file. */
    if (fd == STDOUT_FILENO)
        fflush(stdout);
    else
        fflush(stderr);

    dup2(saved, fd);
    close(saved);

    FILE *rf = fopen(capfile, "rb");
    if (!rf)
        return NULL;
    fseek(rf, 0, SEEK_END);
    long len = ftell(rf);
    fseek(rf, 0, SEEK_SET);

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(rf);
        return NULL;
    }
    if (len > 0 && fread(buf, 1, (size_t)len, rf) != (size_t)len) {
        free(buf);
        fclose(rf);
        return NULL;
    }
    buf[len < 0 ? 0 : len] = '\0';
    fclose(rf);

    /* strip \r (Windows text-mode translation) */
    size_t r = 0;
    for (size_t i = 0; buf[i]; i++)
        if (buf[i] != '\r')
            buf[r++] = buf[i];
    buf[r] = '\0';

    return buf;
}

/* Print the captured content on failure (indented, one physical line). */
static void report_capture(const char *cap)
{
    printf("    got:  %s\n", cap ? cap : "(capture failed)");
}

/* ── JSON assertions ──────────────────────────────────────────────── */

/* Parse the captured text as ONE JSON object. Returns the cJSON object
 * (caller must cJSON_Delete) or NULL when the text is not a single
 * parseable JSON object — which is the class of defect this suite exists
 * for. */
static cJSON *parse_single_object(const char *cap)
{
    if (!cap)
        return NULL;

    /* exactly one line: the payload, then nothing but the final newline */
    const char *nl = strchr(cap, '\n');
    if (!nl || nl[1] != '\0')
        return NULL;

    cJSON *root = cJSON_Parse(cap);
    if (!root)
        return NULL;
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

/* Assert the captured line is byte-identical to `want` plus the final
 * newline (i.e. exactly one line, no extra output on the stream). */
static void check_exact_line(const char *cap, const char *want, const char *what)
{
    char full[2048];
    snprintf(full, sizeof full, "%s\n", want);

    checks++;
    if (cap && strcmp(cap, full) == 0) {
        printf("  PASS %s\n", what);
    } else {
        printf("  FAIL %s\n    got:  %s    want: %s\n", what,
               cap ? cap : "(capture failed)", full);
        failures++;
    }
}

/* Assert the captured line parses as JSON and carries the contracted
 * error/code/message fields (message may be ""). */
static void check_error_json(const char *cap, const char *want_error,
                             int want_code, const char *want_msg,
                             const char *what)
{
    cJSON *root = parse_single_object(cap);
    checks++;
    if (!root) {
        printf("  FAIL %s — the line is not a single parseable JSON object\n",
               what);
        report_capture(cap);
        failures++;
        return;
    }

    cJSON *je = cJSON_GetObjectItemCaseSensitive(root, "error");
    cJSON *jc = cJSON_GetObjectItemCaseSensitive(root, "code");
    cJSON *jm = cJSON_GetObjectItemCaseSensitive(root, "message");

    int ok = cJSON_IsString(je) && strcmp(je->valuestring, want_error) == 0 &&
             cJSON_IsNumber(jc) && jc->valueint == want_code &&
             cJSON_IsString(jm) && strcmp(jm->valuestring, want_msg) == 0;

    if (ok) {
        printf("  PASS %s\n", what);
    } else {
        printf("  FAIL %s (error/code/message mismatch)\n", what);
        report_capture(cap);
        failures++;
    }
    cJSON_Delete(root);
}

/* ── scenario driver ──────────────────────────────────────────────── */

/* Run one ERROR emitter call against `fd`, then assert the exact line, the
 * error/code/message JSON shape and the returned exit code. `want_rc` < 0
 * means "the emitter is a void emitter, do not assert a code". */
static void scenario(int fd, emit_call_t *call, const char *want_line,
                     const char *want_error, int want_code,
                     const char *want_msg, int want_rc, const char *what)
{
    char *cap = capture(fd, emit_call, call);

    if (!cap) {
        printf("  SKIP %s — capture unavailable\n", what);
        checks++;
        return;
    }

    check_exact_line(cap, want_line, what);
    check_error_json(cap, want_error, want_code, want_msg,
                     "same line parses as the contracted JSON");

    if (want_rc >= 0)
        check_int(call->rc_out, want_rc,
                  "the emitter returns the exit code it printed");

    free(cap);
}

/* Run one call and assert only the exact captured line — for the emitters
 * whose payload has no error/code/message keys (the check verdict). Their
 * JSON shape is asserted separately in test_check_verdict_shape. */
static void scenario_line(int fd, emit_call_t *call, const char *want_line,
                          const char *what)
{
    char *cap = capture(fd, emit_call, call);

    if (!cap) {
        printf("  SKIP %s — capture unavailable\n", what);
        checks++;
        return;
    }

    check_exact_line(cap, want_line, what);
    free(cap);
}

/* ── 1. emit_runner_error: the ACTA_RUNNER_ERROR contract ─────────── */

static void test_runner_error(void)
{
    printf("\n-- emit_runner_error: {\"error\":\"ACTA_RUNNER_ERROR\","
           "\"code\":<exit code>,\"message\":…} on stderr --\n");

    emit_call_t c;
    char want[2048];

    /* The exact live failure line of the Strata session (unsupported
     * backend value, EXIT_INVALID before any HTTP call). */
    const char *m1 = "model revision 1 has unsupported backend 'llama' "
                     "(expected llamacpp or openai)";
    snprintf(want, sizeof want,
             "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":4,\"message\":\"%s\"}",
             m1);
    c = (emit_call_t){ CALL_EMIT_RUNNER_ERROR, EXIT_INVALID, m1,
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c, want, "ACTA_RUNNER_ERROR", EXIT_INVALID,
             m1, EXIT_INVALID,
             "unsupported-backend line: {\"error\":… with the OPENING QUOTE");

    const char *m2 = "backend transport failure: allocation failure";
    snprintf(want, sizeof want,
             "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":12,\"message\":\"%s\"}",
             m2);
    c = (emit_call_t){ CALL_EMIT_RUNNER_ERROR, EXIT_HTTP, m2, NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c, want, "ACTA_RUNNER_ERROR", EXIT_HTTP,
             m2, EXIT_HTTP, "HTTP line (code 12 == exit code)");

    const char *m3 = "backend call timed out after 1 s";
    snprintf(want, sizeof want,
             "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":13,\"message\":\"%s\"}",
             m3);
    c = (emit_call_t){ CALL_EMIT_RUNNER_ERROR, EXIT_TIMEOUT, m3,
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c, want, "ACTA_RUNNER_ERROR", EXIT_TIMEOUT,
             m3, EXIT_TIMEOUT, "timeout line (code 13 == exit code)");

    const char *m4 = "execution cancelled";
    snprintf(want, sizeof want,
             "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":14,\"message\":\"%s\"}",
             m4);
    c = (emit_call_t){ CALL_EMIT_RUNNER_ERROR, EXIT_CANCELED, m4,
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c, want, "ACTA_RUNNER_ERROR", EXIT_CANCELED,
             m4, EXIT_CANCELED, "cancel line (code 14 == exit code)");

    /* NULL-safe: a NULL message is an empty string, never a crash and
     * never a missing field. */
    c = (emit_call_t){ CALL_EMIT_RUNNER_ERROR, EXIT_INVALID, NULL,
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":4,\"message\":\"\"}",
             "ACTA_RUNNER_ERROR", EXIT_INVALID, "", EXIT_INVALID,
             "NULL message -> \"message\":\"\"");

    /* JSON escaping: quote, backslash, newline, tab, control byte.
     * (The \x01 is written as its own literal: \x01end would be one greedy
     * hex escape, 0x1E followed by "nd".) */
    const char *raw = "say \"hi\"\nback\\slash\ttab\x01" "end";
    c = (emit_call_t){ CALL_EMIT_RUNNER_ERROR, EXIT_INVALID, raw,
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":4,"
             "\"message\":\"say \\\"hi\\\"\\nback\\\\slash\\ttab\\u0001end\"}",
             "ACTA_RUNNER_ERROR", EXIT_INVALID, raw, EXIT_INVALID,
             "message is JSON-escaped (quote, backslash, \\n, \\t, \\u0001)");
}

/* ── 2. finish_db_error: the ACTA_DB_ERR_* contract ───────────────── */

static void test_finish_db_error(void)
{
    printf("\n-- finish_db_error: {\"error\":\"ACTA_DB_ERR_<NAME>\","
           "\"code\":<rc>,\"message\":…} + map_rc_to_exit(rc) --\n");

    struct {
        int         rc;
        const char *name;
        int         exit_code;
    } table[] = {
        { ACTA_DB_ERR_NOT_FOUND,  "ACTA_DB_ERR_NOT_FOUND",  EXIT_NOT_FOUND },
        { ACTA_DB_ERR_SQL,        "ACTA_DB_ERR_SQL",        EXIT_SQL       },
        { ACTA_DB_ERR_ALLOC,      "ACTA_DB_ERR_ALLOC",      EXIT_ALLOC     },
        { ACTA_DB_ERR_INVALID,    "ACTA_DB_ERR_INVALID",    EXIT_INVALID   },
        { ACTA_DB_ERR_INVALID_DB, "ACTA_DB_ERR_INVALID_DB", EXIT_INVALID   },
        { ACTA_DB_ERR_DUPLICATE,  "ACTA_DB_ERR_DUPLICATE",  EXIT_INVALID   },
        { ACTA_DB_ERR_FK,         "ACTA_DB_ERR_FK",         EXIT_INVALID   },
    };

    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        emit_call_t c = { CALL_FINISH_DB_ERROR, table[i].rc,
                          "detail text", NULL, NULL, 0 };
        char want[256];
        snprintf(want, sizeof want,
                 "{\"error\":\"%s\",\"code\":%d,\"message\":\"detail text\"}",
                 table[i].name, table[i].rc);
        scenario(STDERR_FILENO, &c, want, table[i].name, table[i].rc,
                 "detail text", table[i].exit_code,
                 table[i].name);
    }

    /* ACTA_DB_OK is reported as ACTA_DB_ERR_INVALID (documented): an
     * emitter must never print code 0 as an error. */
    emit_call_t c = { CALL_FINISH_DB_ERROR, ACTA_DB_OK, "no error",
                      NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
             "\"message\":\"no error\"}",
             "ACTA_DB_ERR_INVALID", ACTA_DB_ERR_INVALID, "no error",
             EXIT_INVALID, "ACTA_DB_OK is reported as ACTA_DB_ERR_INVALID");

    /* An unknown code is normalized the same way. */
    c = (emit_call_t){ CALL_FINISH_DB_ERROR, -99, "weird", NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
             "\"message\":\"weird\"}",
             "ACTA_DB_ERR_INVALID", ACTA_DB_ERR_INVALID, "weird",
             EXIT_INVALID, "unknown rc is reported as ACTA_DB_ERR_INVALID");
}

/* ── 3. emit_error / emit_not_found / finish_op_error ─────────────── */

static void test_other_emitters(void)
{
    printf("\n-- emit_error / emit_not_found / finish_op_error --\n");

    emit_call_t c;
    char want[256];

    const char *m = "unknown option '--nope' (see --help)";
    snprintf(want, sizeof want,
             "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,\"message\":\"%s\"}",
             m);
    c = (emit_call_t){ CALL_EMIT_ERROR, 0, m, NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c, want, "ACTA_DB_ERR_INVALID",
             ACTA_DB_ERR_INVALID, m, -1, "emit_error (CLI usage error)");

    snprintf(want, sizeof want,
             "{\"error\":\"ACTA_DB_ERR_NOT_FOUND\",\"code\":-1,"
             "\"message\":\"execution not found\"}");
    c = (emit_call_t){ CALL_EMIT_NOT_FOUND, 0, "execution", NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c, want, "ACTA_DB_ERR_NOT_FOUND",
             ACTA_DB_ERR_NOT_FOUND, "execution not found", EXIT_NOT_FOUND,
             "emit_not_found(\"execution\")");

    c = (emit_call_t){ CALL_EMIT_NOT_FOUND, 0, NULL, NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_DB_ERR_NOT_FOUND\",\"code\":-1,"
             "\"message\":\"entity not found\"}",
             "ACTA_DB_ERR_NOT_FOUND", ACTA_DB_ERR_NOT_FOUND,
             "entity not found", EXIT_NOT_FOUND,
             "emit_not_found(NULL) -> \"entity not found\"");

    /* finish_op_error with a NULL db: the detail is the documented
     * "(no detail)" placeholder, the op name is still there. */
    c = (emit_call_t){ CALL_FINISH_OP_ERROR, ACTA_DB_ERR_SQL, "claim",
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_DB_ERR_SQL\",\"code\":-2,"
             "\"message\":\"claim failed: (no detail)\"}",
             "ACTA_DB_ERR_SQL", ACTA_DB_ERR_SQL,
             "claim failed: (no detail)", EXIT_SQL,
             "finish_op_error(NULL db) -> \"<op> failed: (no detail)\"");

    c = (emit_call_t){ CALL_FINISH_OP_ERROR, ACTA_DB_ERR_INVALID, NULL,
                       NULL, NULL, 0 };
    scenario(STDERR_FILENO, &c,
             "{\"error\":\"ACTA_DB_ERR_INVALID\",\"code\":-4,"
             "\"message\":\"operation failed: (no detail)\"}",
             "ACTA_DB_ERR_INVALID", ACTA_DB_ERR_INVALID,
             "operation failed: (no detail)", EXIT_INVALID,
             "finish_op_error(NULL op) -> \"operation failed: …\"");
}

/* ── 4. emit_check_verdict: the stdout verdict line ────────────────── */

static void test_check_verdict(void)
{
    printf("\n-- emit_check_verdict: the single JSON verdict line on stdout --\n");

    emit_call_t c;

    c = (emit_call_t){ CALL_VERDICT_OK, 204800, "qwen3.8-flash-next-iq3_xxs",
                       "http://127.0.0.1:8080", NULL, 0 };
    scenario_line(STDOUT_FILENO, &c,
             "{\"ok\":true,\"model\":\"qwen3.8-flash-next-iq3_xxs\","
             "\"max_context\":204800,\"base_url\":\"http://127.0.0.1:8080\"}",
             "check verdict: success line on stdout");

    c = (emit_call_t){ CALL_VERDICT_FAIL, 0, "m1", "http://172.28.160.1:8080",
                       "server unreachable", 0 };
    scenario_line(STDOUT_FILENO, &c,
             "{\"ok\":false,\"model\":\"m1\","
             "\"base_url\":\"http://172.28.160.1:8080\","
             "\"verdict\":\"server unreachable\"}",
             "check verdict: failure line carries \"verdict\"");

    c = (emit_call_t){ CALL_VERDICT_FAIL, 0, NULL, NULL, NULL, 0 };
    scenario_line(STDOUT_FILENO, &c,
             "{\"ok\":false,\"model\":\"\",\"base_url\":\"\",\"verdict\":\"\"}",
             "check verdict: NULL model/base/verdict -> empty strings");
}

/* The verdict scenarios above assert the exact line only (the verdict line
 * has no error/code/message keys); assert their JSON shape separately. */
static void test_check_verdict_shape(void)
{
    printf("\n-- the verdict line is parseable JSON with the contracted keys --\n");

    emit_call_t c = { CALL_VERDICT_OK, 4096, "m", "http://h:1", NULL, 0 };
    char *cap = capture(STDOUT_FILENO, emit_call, &c);
    cJSON *root = parse_single_object(cap);
    checks++;
    if (root) {
        cJSON *ok  = cJSON_GetObjectItemCaseSensitive(root, "ok");
        cJSON *mc  = cJSON_GetObjectItemCaseSensitive(root, "max_context");
        cJSON *vd  = cJSON_GetObjectItemCaseSensitive(root, "verdict");
        check(cJSON_IsTrue(ok) && cJSON_IsNumber(mc) && mc->valueint == 4096 &&
              vd == NULL,
              "success verdict: ok=true, max_context set, no verdict key");
        cJSON_Delete(root);
    } else {
        printf("  FAIL success verdict line is not parseable JSON\n");
        report_capture(cap);
        failures++;
    }
    free(cap);

    c = (emit_call_t){ CALL_VERDICT_FAIL, 0, "m", "http://h:1",
                       "model not served", 0 };
    cap = capture(STDOUT_FILENO, emit_call, &c);
    root = parse_single_object(cap);
    checks++;
    if (root) {
        cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
        cJSON *vd = cJSON_GetObjectItemCaseSensitive(root, "verdict");
        check(cJSON_IsFalse(ok) && cJSON_IsString(vd) &&
              strcmp(vd->valuestring, "model not served") == 0,
              "failure verdict: ok=false, verdict string set");
        cJSON_Delete(root);
    } else {
        printf("  FAIL failure verdict line is not parseable JSON\n");
        report_capture(cap);
        failures++;
    }
    free(cap);
}

/* ── 5. the harness itself rejects the malformed line ─────────────── */

static void test_harness_rejects_the_original_defect(void)
{
    printf("\n-- the assertion machinery rejects the pre-fix line --\n");

    /* What emit_runner_error() printed before the fix: the opening quote of
     * the "error" key was eaten by the adjacent-literal split. This is NOT
     * valid JSON, so both assertions above must reject it — otherwise this
     * suite would not have caught the defect. */
    const char *broken =
        "{error\":\"ACTA_RUNNER_ERROR\",\"code\":4,\"message\":\"m\"}\n";

    checks++;
    cJSON *root = parse_single_object(broken);
    if (root) {
        printf("  FAIL the harness accepted the malformed line\n");
        failures++;
        cJSON_Delete(root);
    } else {
        printf("  PASS the malformed {error\":\"… line is rejected\n");
    }

    /* A valid line with the same content must be accepted — the rejection
     * above is about the missing quote, not about the content. */
    checks++;
    root = parse_single_object(
        "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":4,\"message\":\"m\"}\n");
    if (root) {
        printf("  PASS the fixed {\"error\":\"… line is accepted\n");
        cJSON_Delete(root);
    } else {
        printf("  FAIL the harness rejected a valid line\n");
        failures++;
    }

    /* Two lines on one stream is a contract break too. */
    checks++;
    root = parse_single_object(
        "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":4,\"message\":\"m\"}\n"
        "{\"error\":\"ACTA_RUNNER_ERROR\",\"code\":4,\"message\":\"m\"}\n");
    if (root) {
        printf("  FAIL the harness accepted a two-line capture\n");
        failures++;
        cJSON_Delete(root);
    } else {
        printf("  PASS a two-line capture is rejected\n");
    }
}

int main(void)
{
    /* Scratch dir for the capture file: TMPDIR / TEMP, else the cwd (the
     * suites always run from acta_runner/). */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0])
        tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0])
        tmpdir = getenv("TMP");
    if (!tmpdir || !tmpdir[0])
        tmpdir = ".";
    if (strlen(tmpdir) + 32 >= sizeof capfile)
        tmpdir = ".";

#ifdef _WIN32
    snprintf(capfile, sizeof capfile, "%s\\acta_test_util_capture.txt", tmpdir);
#else
    snprintf(capfile, sizeof capfile, "%s/acta_test_util_capture.txt", tmpdir);
#endif

    printf("acta_runner emitter contract suite (capture file: %s)\n", capfile);

    test_runner_error();
    test_finish_db_error();
    test_other_emitters();
    test_check_verdict();
    test_check_verdict_shape();
    test_harness_rejects_the_original_defect();

    remove(capfile);

    printf("\n%d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
