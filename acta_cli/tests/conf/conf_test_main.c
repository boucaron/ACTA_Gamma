/* Config-file helper unit tests (acta_conf, acta_db) — no DB, no argv.
 *
 * Pins the per-machine config-file contract at the unit level:
 *   - acta_conf_parse: at-most-four-keys contract, fail-closed on
 *     not-an-object / unknown key / wrong type / malformed JSON
 *     (mirror of the model `configuration` blob check in run.c);
 *   - acta_conf_api_key_status: $OPENAI_API_KEY (if set, even empty)
 *     wins over the file's "api_key" (file = fallback, not a second
 *     channel); unset + no file key -> the existing hard error state;
 *   - acta_conf_read: missing/unreadable file = fallback unavailable
 *     (not an error), readable-but-malformed = fail-closed hard error,
 *     and (POSIX) a group/other-readable file is refused fail-closed
 *     BEFORE its contents are read (0600 contract);
 *   - acta_conf_default_path: per-platform string (same app-data dir
 *     as the default DB file);
 *   - acta_conf_resolve_max_chars / _timeout: file-over-builtin order
 *     (work item 4).
 *
 * The end-to-end cmd_run behaviour (env-set-wins, file fallback,
 * bad-permissions refusal, missing-file + unset-env hard error) is
 * pinned by acta_runner/tests/run/test_conf.c; the DB-path order
 * with/without the file by acta_cli/tests/dbpath (work item 3).
 */
#include "conf.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

/* ── minimal assertion harness ────────────────────────────────────── */

static int g_fail  = 0;
static int g_total = 0;

static void check(int cond, const char *what)
{
    g_total++;
    if (!cond) {
        g_fail++;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

/* NULL-safe string equality: acta_conf_read leaves the struct fully
 * zeroed on failure, so a failed read must not strcmp(NULL, ...). */
static int str_eq(const char *a, const char *b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

#define T(cond)          check((cond) != 0, #cond)
#define TSTREQ(a, b)     check(str_eq((a), (b)), #a " == " #b)

static void env_set(const char *k, const char *v)
{
#ifdef _WIN32
    /* MinGW: no setenv/unsetenv; "k=" (empty value) unsets the variable. */
    char buf[512];
    if (v)
        snprintf(buf, sizeof buf, "%s=%s", k, v);
    else
        snprintf(buf, sizeof buf, "%s=", k);
    _putenv(buf);
#else
    if (v)
        setenv(k, v, 1);
    else
        unsetenv(k);
#endif
}

/* ── acta_conf_parse ──────────────────────────────────────────────── */

static void test_parse(void)
{
    acta_conf_t conf;
    char *err = NULL;

    /* All four keys present, well-formed. */
    T(acta_conf_parse(
          "{\"api_key\": \"k1\", \"db\": \"/x.db\", "
          "\"max_chars\": 42, \"timeout\": 60}",
          &conf, &err) == 0);
    T(err == NULL);
    TSTREQ(conf.api_key, "k1");
    TSTREQ(conf.db, "/x.db");
    T(conf.max_chars == 42);
    T(conf.timeout == 60);
    acta_conf_free(&conf);

    /* Empty object: everything absent (strings NULL, ints 0). */
    T(acta_conf_parse("{}", &conf, &err) == 0);
    T(err == NULL);
    T(conf.api_key == NULL);
    T(conf.db == NULL);
    T(conf.max_chars == 0);
    T(conf.timeout == 0);
    acta_conf_free(&conf);

    /* Trailing newline is fine (whitespace), trailing garbage is not. */
    T(acta_conf_parse("{\"timeout\": 5}\n", &conf, &err) == 0);
    T(conf.timeout == 5);
    acta_conf_free(&conf);
    T(acta_conf_parse("{} x", &conf, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;

    /* Root not an object. */
    T(acta_conf_parse("[1,2]", &conf, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;

    /* Unknown top-level key (typo) -> contract violation. */
    T(acta_conf_parse("{\"api_keys\": \"k\"}", &conf, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;

    /* Wrong types. */
    T(acta_conf_parse("{\"api_key\": 42}", &conf, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;
    T(acta_conf_parse("{\"db\": [1]}", &conf, &err) == -1);
    free(err); err = NULL;
    T(acta_conf_parse("{\"max_chars\": \"42\"}", &conf, &err) == -1);
    free(err); err = NULL;
    T(acta_conf_parse("{\"timeout\": 1.5}", &conf, &err) == -1);
    free(err); err = NULL;
    T(acta_conf_parse("{\"max_chars\": 0}", &conf, &err) == -1);
    free(err); err = NULL;
    T(acta_conf_parse("{\"timeout\": -5}", &conf, &err) == -1);
    free(err); err = NULL;

    /* Malformed JSON. */
    T(acta_conf_parse("{nope", &conf, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;

    /* NULL input. */
    T(acta_conf_parse(NULL, &conf, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;

    /* Failure leaves the struct fully zeroed (caller frees nothing). */
    conf.api_key = "leak-canary";  /* must be zeroed by the parser */
    T(acta_conf_parse("{}", &conf, &err) == 0);
    T(conf.api_key == NULL);
    acta_conf_free(&conf);

    /* NULL err_msg is tolerated; NULL out is a hard error. */
    T(acta_conf_parse("{}", &conf, NULL) == 0);
    acta_conf_free(&conf);
    T(acta_conf_parse("{}", NULL, &err) == -1);
}

/* ── acta_conf_api_key_status ─────────────────────────────────────── */

static void test_api_key_status(void)
{
    const char *msg = "canary";

    /* env-set-wins: env set (non-empty) -> OK regardless of file key. */
    T(acta_conf_api_key_status("env-key", "file-key", &msg) == ACTA_KEY_OK);
    T(msg == NULL);

    /* env set to EMPTY still wins: warning, not ok, not error. */
    T(acta_conf_api_key_status("", "file-key", &msg) == ACTA_KEY_EMPTY_WARN);
    T(msg != NULL && strstr(msg, "OPENAI_API_KEY is empty") != NULL);

    /* File fallback: env unset + file key present -> OK. */
    T(acta_conf_api_key_status(NULL, "file-key", &msg) == ACTA_KEY_OK);
    T(msg == NULL);

    /* Missing file + env unset (file_key NULL): the existing hard error. */
    T(acta_conf_api_key_status(NULL, NULL, &msg) == ACTA_KEY_UNSET_ERR);
    T(msg != NULL && strstr(msg, "OPENAI_API_KEY is not set") != NULL);
    T(strstr(msg, "api_key") != NULL);

    /* File key present but empty -> warning (file variant message). */
    T(acta_conf_api_key_status(NULL, "", &msg) == ACTA_KEY_EMPTY_WARN);
    T(msg != NULL && strstr(msg, "config file") != NULL);

    /* NULL msg pointer is tolerated. */
    T(acta_conf_api_key_status(NULL, NULL, NULL) == ACTA_KEY_UNSET_ERR);
}

/* ── acta_conf_default_path ───────────────────────────────────────── */

static void test_default_path(void)
{
#ifdef _WIN32
    env_set("APPDATA", "C:/Users/t/AppData/Roaming");
    TSTREQ(acta_conf_default_path(),
           "C:/Users/t/AppData/Roaming\\ACTA_Gamma\\ACTA_Gamma.conf");
    env_set("APPDATA", NULL);
    /* Platform base unresolvable -> last-resort ./ACTA_Gamma.conf. */
    TSTREQ(acta_conf_default_path(), "./ACTA_Gamma.conf");
#else
    env_set("XDG_DATA_HOME", "/srv/data");
    TSTREQ(acta_conf_default_path(), "/srv/data/ACTA_Gamma/ACTA_Gamma.conf");
    env_set("XDG_DATA_HOME", NULL);
    env_set("HOME", "/home/t");
    TSTREQ(acta_conf_default_path(),
           "/home/t/.local/share/ACTA_Gamma/ACTA_Gamma.conf");
    env_set("HOME", NULL);
    TSTREQ(acta_conf_default_path(), "./ACTA_Gamma.conf");
#endif
}

/* ── acta_conf_read ───────────────────────────────────────────────── */

static int write_file(const char *path, const char *content, int mode)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
    /* The permission gate refuses group/other-readable files; the tests
     * pick the mode explicitly (0600 = contract mode). */
    if (chmod(path, mode) != 0)
        return 0;
    return 1;
}

static void test_read(void)
{
    char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0])
        tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0])
        tmpdir = ".";
    if (strlen(tmpdir) + 80 > 512) {
        fprintf(stderr, "SKIP: temp dir too long\n");
        return;
    }
    char path[512];
    snprintf(path, sizeof path,
#ifdef _WIN32
             "%s\\acta_conf_test.conf", tmpdir
#else
             "%s/acta_conf_test.conf", tmpdir
#endif
             );

    /* Start from a clean slate: a stale file left by a previously
     * crashed run must not leak into the missing-file case. */
    remove(path);

    acta_conf_t conf;
    int missing = 0;
    char *err = NULL;

    /* Missing file: not an error; *missing = 1; struct left zeroed. */
    missing = -1;
    T(acta_conf_read(path, &conf, &missing, &err) == 0);
    T(missing == 1);
    T(err == NULL);
    T(conf.api_key == NULL && conf.db == NULL);
    T(conf.max_chars == 0 && conf.timeout == 0);

    /* Valid 0600 file parses. */
    T(write_file(path,
                 "{\"api_key\": \"k1\", \"max_chars\": 42, "
                 "\"timeout\": 60}", 0600));
    missing = -1;
    err = NULL;
    T(acta_conf_read(path, &conf, &missing, &err) == 0);
    T(missing == 0);
    T(err == NULL);
    TSTREQ(conf.api_key, "k1");
    T(conf.max_chars == 42);
    T(conf.timeout == 60);
    acta_conf_free(&conf);

    /* Readable but malformed: fail-closed hard error. */
    T(write_file(path, "{nope", 0600));
    err = NULL;
    T(acta_conf_read(path, &conf, &missing, &err) == -1);
    T(err != NULL);
    free(err); err = NULL;
    T(conf.api_key == NULL && conf.db == NULL);
    T(conf.max_chars == 0 && conf.timeout == 0);

    /* Bad permissions: a group/other-readable file is refused
     * fail-closed BEFORE the contents are read, whatever its mode bits
     * give read access to group or other.  POSIX-only assertion: on
     * Windows the mode bits are meaningless (always 0666) and the gate
     * warns and reads the file anyway (best-effort). */
#ifndef _WIN32
    T(write_file(path, "{\"api_key\": \"k1\"}", 0644));
    err = NULL;
    T(acta_conf_read(path, &conf, &missing, &err) == -1);
    T(err != NULL &&
          strstr(err, "0600") != NULL);
    free(err); err = NULL;
    T(missing == 0);   /* the file EXISTS; it is refused, not missing */
    T(conf.api_key == NULL);   /* contents never read */

    T(write_file(path, "{\"api_key\": \"k1\"}", 0604));
    T(acta_conf_read(path, &conf, &missing, &err) == -1);
    free(err); err = NULL;
    T(write_file(path, "{\"api_key\": \"k1\"}", 0640));
    T(acta_conf_read(path, &conf, &missing, &err) == -1);
    free(err); err = NULL;
    T(write_file(path, "not json", 0444));
    T(acta_conf_read(path, &conf, &missing, &err) == -1);
    free(err); err = NULL;
#endif

    remove(path);
}

/* ── resolution helpers (work item 4) ─────────────────────────────── */

static void test_resolve(void)
{
    acta_conf_t conf;
    memset(&conf, 0, sizeof conf);

    /* Absent (zeroed) conf -> built-in defaults. */
    T(acta_conf_resolve_max_chars(&conf) == ACTA_CONF_DEFAULT_MAX_CHARS);
    T(acta_conf_resolve_max_chars(NULL) == ACTA_CONF_DEFAULT_MAX_CHARS);
    T(acta_conf_resolve_timeout(&conf, 0) == ACTA_CONF_DEFAULT_TIMEOUT);

    /* File value wins over the built-in default. */
    conf.max_chars = 42;
    conf.timeout = 60;
    T(acta_conf_resolve_max_chars(&conf) == 42);
    T(acta_conf_resolve_timeout(&conf, 0) == 60);

    /* The --timeout flag wins over the file value. */
    T(acta_conf_resolve_timeout(&conf, 120) == 120);
    T(acta_conf_resolve_timeout(&conf, 0) == 60);

    acta_conf_free(&conf);
}

int main(void)
{
    test_parse();
    test_api_key_status();
    test_default_path();
    test_read();
    test_resolve();

    if (g_fail == 0) {
        printf("PASS: all conf tests passed (%d assertions)\n", g_total);
        return 0;
    }
    printf("FAIL: %d of %d assertion(s) failed\n", g_fail, g_total);
    return 1;
}
