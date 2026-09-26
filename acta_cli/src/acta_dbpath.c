/*
 * acta_dbpath.c — DB path resolution shared by acta_cli and acta_runner.
 * See acta_dbpath.h for the resolution-order contract; the .h/.c pair
 * is duplicated verbatim in acta_runner/{include,src} — keep them in
 * lockstep.
 */
#include "acta_dbpath.h"

#include <conf.h>   /* acta_conf_read / acta_conf_default_path */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define ACTA_DBPATH_MAX 4096

/* Platform app-data base directory (no trailing slash), or NULL when
 * it cannot be resolved.  Windows: %APPDATA% (the roaming AppData
 * location the GUI uses).  POSIX: $XDG_DATA_HOME, else $HOME/.local/share.
 * No organization-name segment is appended — that is what keeps the
 * path free of the org name. */
static const char *appdata_base(void)
{
#ifdef _WIN32
    const char *p = getenv("APPDATA");
    return (p && p[0]) ? p : NULL;
#else
    const char *p = getenv("XDG_DATA_HOME");
    if (p && p[0])
        return p;
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return NULL;
    static char fallback[ACTA_DBPATH_MAX];
    size_t n = strlen(home);
    if (n + 1 + sizeof("/.local/share") - 1 + 1 > sizeof fallback)
        return NULL;
    snprintf(fallback, sizeof fallback, "%s/.local/share", home);
    return fallback;
#endif
}

const char *acta_db_default_db_path(void)
{
    static char path[ACTA_DBPATH_MAX];
    const char *base = appdata_base();
    if (!base)
        return "./acta.db";

    size_t n = strlen(base);
    if (n + 1 + sizeof("/ACTA_Gamma/acta.db") > sizeof path)
        return "./acta.db";

#ifdef _WIN32
    snprintf(path, sizeof path, "%s\\ACTA_Gamma\\acta.db", base);
#else
    snprintf(path, sizeof path, "%s/ACTA_Gamma/acta.db", base);
#endif
    return path;
}

const char *acta_db_resolve_db_path(const char *flag, char **err_msg)
{
    if (err_msg)
        *err_msg = NULL;
    if (flag && flag[0])
        return flag;

    /* Config-file step (docs/cli_spec.md, DB-file resolution):
     * "db" in ACTA_Gamma.conf, in the same app-data directory as the
     * default DB file.  The file is consulted even when $ACTA_DB is
     * set (a readable-but-malformed file is a fail-closed hard error,
     * the same rules as acta_conf_parse; the precedence --db ->
     * $ACTA_DB -> file -> platform default applies to the resolved
     * value only, so a stray file can still never retarget a run that
     * named its DB).  A missing or unreadable file is simply
     * unavailable (fall through); an empty "db" value is treated as
     * absent. */
    acta_conf_t conf;
    int missing = 0;
    char *conf_err = NULL;
    if (acta_conf_read(acta_conf_default_path(), &conf, &missing,
                       &conf_err) != 0) {
        if (err_msg)
            *err_msg = conf_err; /* malloc'd; the caller frees */
        else
            free(conf_err);
        return NULL;
    }

    const char *env = getenv("ACTA_DB");
    if (env && env[0]) {
        acta_conf_free(&conf);
        return env;
    }

    if (conf.db && conf.db[0]) {
        static char path[ACTA_DBPATH_MAX];
        size_t n = strlen(conf.db);
        if (n + 1 > sizeof path) {
            char *m = (char *)malloc(256);
            if (m)
                snprintf(m, 256,
                         "config file 'db' path is %zu bytes long; "
                         "maximum is %d",
                         n, (int)(sizeof path - 1));
            acta_conf_free(&conf);
            if (err_msg)
                *err_msg = m;
            return NULL;
        }
        snprintf(path, sizeof path, "%s", conf.db); /* copy before free */
        acta_conf_free(&conf);
        return path;
    }
    acta_conf_free(&conf);
    return acta_db_default_db_path();
}

int acta_db_legacy_db_hint(char *hint, size_t hint_size)
{
    if (!hint || hint_size == 0)
        return 0;

    int exists;
#ifdef _WIN32
    exists = (_access("./acta.db", 0) == 0);
#else
    exists = (access("./acta.db", F_OK) == 0);
#endif
    if (!exists) {
        hint[0] = '\0';
        return 0;
    }

    snprintf(hint, hint_size,
             "note: ./acta.db exists in the current directory — "
             "pass --db ./acta.db to use it");
    return 1;
}
