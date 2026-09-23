/* DB path resolution tests (acta_dbpath) — no DB, no argv seam.
 *
 * Pins the resolution order from docs/cli_spec.md ("DB file"):
 *   --db flag → $ACTA_DB → config file "db" → platform app-data
 *   default → ./acta.db
 * plus the exact per-platform default string (which must match the
 * GUI's MainWindow::defaultDbPath byte-for-byte), the config-file step
 * (docs/plans/acta-config-file.md, work item 3), and the legacy
 * ./acta.db hint behaviour.
 */
#include "acta_dbpath.h"

#include <conf.h>   /* acta_conf_default_path: the resolver's file location */

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

#define T(cond)          check((cond) != 0, #cond)
#define TSTREQ(a, b)     check(strcmp((a), (b)) == 0, #a " == " #b)

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

/* ── resolution order ─────────────────────────────────────────────── */

static void order_flag_wins(void)
{
    env_set("ACTA_DB", "/tmp/env.db");
#ifdef _WIN32
    env_set("APPDATA", "C:/Users/t/AppData/Roaming");
#else
    env_set("XDG_DATA_HOME", "/srv/data");
    env_set("HOME", "/home/t");
#endif
    TSTREQ(acta_db_resolve_db_path("/tmp/flag.db", NULL), "/tmp/flag.db");
    /* the flag pointer is returned as-is */
    TSTREQ(acta_db_resolve_db_path("/tmp/flag.db", NULL),
           acta_db_resolve_db_path("/tmp/flag.db", NULL));
    env_set("ACTA_DB", NULL);
#ifdef _WIN32
    env_set("APPDATA", NULL);
#else
    env_set("XDG_DATA_HOME", NULL);
    env_set("HOME", NULL);
#endif
}

static void order_env_wins(void)
{
#ifdef _WIN32
    env_set("APPDATA", "C:/Users/t/AppData/Roaming");
#else
    env_set("XDG_DATA_HOME", "/srv/data");
    env_set("HOME", "/home/t");
#endif
    env_set("ACTA_DB", "/tmp/env.db");
    TSTREQ(acta_db_resolve_db_path(NULL, NULL), "/tmp/env.db");
    /* empty $ACTA_DB is treated as unset → platform default */
    env_set("ACTA_DB", "");
#ifdef _WIN32
    TSTREQ(acta_db_resolve_db_path(NULL, NULL),
           "C:/Users/t/AppData/Roaming\\ACTA_Gamma\\acta.db");
#else
    TSTREQ(acta_db_resolve_db_path(NULL, NULL),
           "/srv/data/ACTA_Gamma/acta.db");
#endif
    env_set("ACTA_DB", NULL);
#ifdef _WIN32
    env_set("APPDATA", NULL);
#else
    env_set("XDG_DATA_HOME", NULL);
    env_set("HOME", NULL);
#endif
}

/* ── exact per-platform default string (must match the GUI) ────────── */

static void default_windows(void)
{
#ifdef _WIN32
    env_set("ACTA_DB", NULL);
    env_set("HOME", "C:/Users/t");
    env_set("APPDATA", "C:/Users/t/AppData/Roaming");
    TSTREQ(acta_db_resolve_db_path(NULL, NULL),
           "C:/Users/t/AppData/Roaming\\ACTA_Gamma\\acta.db");
    TSTREQ(acta_db_default_db_path(),
           "C:/Users/t/AppData/Roaming\\ACTA_Gamma\\acta.db");
    /* no org-name segment anywhere in the path */
    T(strstr(acta_db_resolve_db_path(NULL, NULL), "boucaron") == NULL);
    env_set("APPDATA", NULL);
    /* platform base unresolvable → last-resort ./acta.db */
    TSTREQ(acta_db_resolve_db_path(NULL, NULL), "./acta.db");
    TSTREQ(acta_db_default_db_path(), "./acta.db");
    env_set("ACTA_DB", NULL);
#endif
}

static void default_posix(void)
{
#ifndef _WIN32
    env_set("ACTA_DB", NULL);
    env_set("HOME", "/home/t");
    env_set("XDG_DATA_HOME", NULL);
    /* $HOME/.local/share fallback */
    TSTREQ(acta_db_resolve_db_path(NULL, NULL),
           "/home/t/.local/share/ACTA_Gamma/acta.db");
    TSTREQ(acta_db_default_db_path(),
           "/home/t/.local/share/ACTA_Gamma/acta.db");
    /* $XDG_DATA_HOME wins over $HOME */
    env_set("XDG_DATA_HOME", "/srv/data");
    TSTREQ(acta_db_resolve_db_path(NULL, NULL), "/srv/data/ACTA_Gamma/acta.db");
    TSTREQ(acta_db_default_db_path(), "/srv/data/ACTA_Gamma/acta.db");
    /* no org-name segment anywhere in the path */
    T(strstr(acta_db_resolve_db_path(NULL, NULL), "boucaron") == NULL);
    env_set("XDG_DATA_HOME", NULL);
    env_set("HOME", NULL);
    /* platform base unresolvable → last-resort ./acta.db */
    TSTREQ(acta_db_resolve_db_path(NULL, NULL), "./acta.db");
    TSTREQ(acta_db_default_db_path(), "./acta.db");
    env_set("ACTA_DB", NULL);
#endif
}

/* ── config-file step (work item 3) ─────────────────────────────────── */

static int write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(content, f);
    fclose(f);
#ifndef _WIN32
    /* The resolver's POSIX permission gate (work item 5) refuses
     * group/other-readable files; a default-umask (0644) file would be
     * refused, so pin the contract mode 0600.  On Windows the mode bits
     * are meaningless (always 0666) and the gate is not run. */
    if (chmod(path, 0600) != 0)
        return 0;
#endif
    return 1;
}

/* Create <tmpdir>/acta_dbpath_conf/ and its "ACTA_Gamma" subdir;
 * stores the base dir in base and the full config-file path in conf.
 * Existing dirs (from a previous run) are tolerated.  Returns 1 ok, 0
 * failure. */
static int make_conf_locations(const char *tmpdir, char *base, size_t bs,
                               char *conf, size_t cs)
{
    char gamma[600];
#ifdef _WIN32
    snprintf(base, bs, "%s\\acta_dbpath_conf", tmpdir);
    snprintf(gamma, sizeof gamma, "%s\\ACTA_Gamma", base);
    errno = 0;
    int r1 = _mkdir(base);
    errno = 0;
    int r2 = _mkdir(gamma);
    if ((r1 != 0 && errno != EEXIST) || (r2 != 0 && errno != EEXIST))
        return 0;
    snprintf(conf, cs, "%s\\ACTA_Gamma.conf", gamma);
#else
    snprintf(base, bs, "%s/acta_dbpath_conf", tmpdir);
    snprintf(gamma, sizeof gamma, "%s/ACTA_Gamma", base);
    errno = 0;
    int r1 = mkdir(base, 0755);
    errno = 0;
    int r2 = mkdir(gamma, 0755);
    if ((r1 != 0 && errno != EEXIST) || (r2 != 0 && errno != EEXIST))
        return 0;
    snprintf(conf, cs, "%s/ACTA_Gamma.conf", gamma);
#endif
    return 1;
}

static void conf_db_step(void)
{
    char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0])
        tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0])
        tmpdir = ".";

    char base[512], confpath[700];
    if (strlen(tmpdir) + 80 > sizeof base) {
        fprintf(stderr, "SKIP: temp dir too long\n");
        return;
    }
    if (!make_conf_locations(tmpdir, base, sizeof base,
                             confpath, sizeof confpath)) {
        fprintf(stderr, "SKIP: cannot create config locations\n");
        return;
    }

    /* No --db, no $ACTA_DB: the platform base env vars point at our
     * scratch dir, and the resolver must look at exactly
     * acta_conf_default_path() (same app-data dir as the default DB).
     * No file there yet → missing file is skipped, platform default. */
    env_set("ACTA_DB", NULL);
#ifdef _WIN32
    env_set("APPDATA", base);
#else
    env_set("XDG_DATA_HOME", base);
    env_set("HOME", "/home/t");
#endif

    /* The expected platform default under this scratch base. */
    char expected[700];
#ifdef _WIN32
    snprintf(expected, sizeof expected, "%s\\ACTA_Gamma\\acta.db", base);
#else
    snprintf(expected, sizeof expected, "%s/ACTA_Gamma/acta.db", base);
#endif

    char *err = NULL;
    TSTREQ(confpath, acta_conf_default_path());
    TSTREQ(acta_db_resolve_db_path(NULL, &err), expected);
    T(err == NULL);

    /* The file's "db" wins over the platform default. */
    T(write_file(confpath, "{\"db\": \"/tmp/fromconf.db\"}"));
    TSTREQ(acta_db_resolve_db_path(NULL, &err), "/tmp/fromconf.db");
    T(err == NULL);

    /* $ACTA_DB and the flag still win over the file. */
    env_set("ACTA_DB", "/tmp/env.db");
    TSTREQ(acta_db_resolve_db_path(NULL, &err), "/tmp/env.db");
    TSTREQ(acta_db_resolve_db_path("/tmp/flag.db", &err), "/tmp/flag.db");
    env_set("ACTA_DB", NULL);

    /* Absent "db" key, and an empty "db" value → platform default. */
    T(write_file(confpath, "{}"));
    TSTREQ(acta_db_resolve_db_path(NULL, &err), expected);
    T(write_file(confpath, "{\"db\": \"\"}"));
    TSTREQ(acta_db_resolve_db_path(NULL, &err), expected);
    T(err == NULL);

    /* Fail-closed: a readable-but-malformed file is a hard error, even
     * when $ACTA_DB is set (the file is consulted, not skipped). */
    env_set("ACTA_DB", "/tmp/env.db");
    T(write_file(confpath, "{nope"));
    T(acta_db_resolve_db_path(NULL, &err) == NULL);
    T(err != NULL);
    free(err); err = NULL;

    T(write_file(confpath, "{\"dbb\": \"/tmp/x.db\"}"));
    T(acta_db_resolve_db_path(NULL, &err) == NULL);
    free(err); err = NULL;

    T(write_file(confpath, "{\"db\": 42}"));
    T(acta_db_resolve_db_path(NULL, &err) == NULL);
    free(err); err = NULL;
    env_set("ACTA_DB", NULL);

    /* Missing file again (removed) → platform default. */
    remove(confpath);
    TSTREQ(acta_db_resolve_db_path(NULL, &err), expected);

    /* clean up */
    char gamma[600];
#ifdef _WIN32
    snprintf(gamma, sizeof gamma, "%s\\ACTA_Gamma", base);
    _rmdir(gamma);
    _rmdir(base);
#else
    snprintf(gamma, sizeof gamma, "%s/ACTA_Gamma", base);
    rmdir(gamma);
    rmdir(base);
#endif
#ifdef _WIN32
    env_set("APPDATA", NULL);
#else
    env_set("XDG_DATA_HOME", NULL);
    env_set("HOME", NULL);
#endif
    env_set("ACTA_DB", NULL);
}

/* ── legacy ./acta.db hint ─────────────────────────────────────────── */

static void legacy_hint(void)
{
    char dir[256];
    char saved[256];
    char hint[256];

    char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0])
        tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0])
        tmpdir = ".";
    if (strlen(tmpdir) + 30 > sizeof dir) {
        fprintf(stderr, "SKIP: temp dir too long\n");
        return;
    }
#ifdef _WIN32
    snprintf(dir, sizeof dir, "%s\\acta_dbpath_hint", tmpdir);
#else
    snprintf(dir, sizeof dir, "%s/acta_dbpath_hint", tmpdir);
#endif
#ifdef _WIN32
    /* MinGW's mkdir takes one argument (no POSIX mode parameter). */
    if (_mkdir(dir) != 0) {
        fprintf(stderr, "SKIP: cannot create %s\n", dir);
        return;
    }
#else
    if (mkdir(dir, 0755) != 0) {
        fprintf(stderr, "SKIP: cannot create %s\n", dir);
        return;
    }
#endif

    const char *cwd = getcwd(saved, sizeof saved);
    if (!cwd)
        return;

    if (chdir(dir) != 0) {
        fprintf(stderr, "SKIP: cannot chdir %s\n", dir);
        return;
    }

    /* no ./acta.db yet → no hint */
    T(acta_db_legacy_db_hint(hint, sizeof hint) == 0);

    FILE *f = fopen("acta.db", "w");
    if (f) {
        fclose(f);
        T(acta_db_legacy_db_hint(hint, sizeof hint) == 1);
        T(strstr(hint, "./acta.db") != NULL);
        T(strstr(hint, "--db ./acta.db") != NULL);
    } else {
        T(0);
    }

    /* clean up */
    remove("acta.db");
#ifndef _WIN32
    rmdir(dir);
#endif
    chdir(saved);
}

int main(void)
{
    order_flag_wins();
    order_env_wins();
    default_windows();
    default_posix();
    conf_db_step();
    legacy_hint();

    if (g_fail == 0) {
        printf("PASS: all dbpath tests passed (%d assertions)\n", g_total);
        return 0;
    }
    printf("FAIL: %d of %d assertion(s) failed\n", g_fail, g_total);
    return 1;
}
