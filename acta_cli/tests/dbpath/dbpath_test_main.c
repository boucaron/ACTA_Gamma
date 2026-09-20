/* DB path resolution tests (acta_dbpath) — no DB, no argv seam.
 *
 * Pins the resolution order from docs/cli_spec.md ("DB file"):
 *   --db flag → $ACTA_DB → platform app-data default → ./acta.db
 * plus the exact per-platform default string (which must match the
 * GUI's MainWindow::defaultDbPath byte-for-byte) and the legacy
 * ./acta.db hint behaviour.
 */
#include "acta_dbpath.h"

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
    TSTREQ(acta_db_resolve_db_path("/tmp/flag.db"), "/tmp/flag.db");
    /* the flag pointer is returned as-is */
    TSTREQ(acta_db_resolve_db_path("/tmp/flag.db"),
           acta_db_resolve_db_path("/tmp/flag.db"));
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
    TSTREQ(acta_db_resolve_db_path(NULL), "/tmp/env.db");
    /* empty $ACTA_DB is treated as unset → platform default */
    env_set("ACTA_DB", "");
#ifdef _WIN32
    TSTREQ(acta_db_resolve_db_path(NULL),
           "C:/Users/t/AppData/Roaming\\ACTA Gamma\\acta.db");
#else
    TSTREQ(acta_db_resolve_db_path(NULL),
           "/srv/data/ACTA Gamma/acta.db");
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
    TSTREQ(acta_db_resolve_db_path(NULL),
           "C:/Users/t/AppData/Roaming\\ACTA Gamma\\acta.db");
    /* no org-name segment anywhere in the path */
    T(strstr(acta_db_resolve_db_path(NULL), "boucaron") == NULL);
    env_set("APPDATA", NULL);
    /* platform base unresolvable → last-resort ./acta.db */
    TSTREQ(acta_db_resolve_db_path(NULL), "./acta.db");
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
    TSTREQ(acta_db_resolve_db_path(NULL),
           "/home/t/.local/share/ACTA Gamma/acta.db");
    /* $XDG_DATA_HOME wins over $HOME */
    env_set("XDG_DATA_HOME", "/srv/data");
    TSTREQ(acta_db_resolve_db_path(NULL), "/srv/data/ACTA Gamma/acta.db");
    /* no org-name segment anywhere in the path */
    T(strstr(acta_db_resolve_db_path(NULL), "boucaron") == NULL);
    env_set("XDG_DATA_HOME", NULL);
    env_set("HOME", NULL);
    /* platform base unresolvable → last-resort ./acta.db */
    TSTREQ(acta_db_resolve_db_path(NULL), "./acta.db");
    env_set("ACTA_DB", NULL);
#endif
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
    legacy_hint();

    if (g_fail == 0) {
        printf("PASS: all dbpath tests passed (%d assertions)\n", g_total);
        return 0;
    }
    printf("FAIL: %d of %d assertion(s) failed\n", g_fail, g_total);
    return 1;
}
