/*
 * acta_dbpath.c — DB path resolution shared by acta_cli and acta_runner.
 * See acta_dbpath.h for the resolution-order contract; the .h/.c pair
 * is duplicated verbatim in acta_runner/{include,src} — keep them in
 * lockstep.
 */
#include "acta_dbpath.h"

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

const char *acta_db_resolve_db_path(const char *flag)
{
    if (flag && flag[0])
        return flag;

    const char *env = getenv("ACTA_DB");
    if (env && env[0])
        return env;

    static char path[ACTA_DBPATH_MAX];
    const char *base = appdata_base();
    if (!base)
        return "./acta.db";

    size_t n = strlen(base);
    if (n + 1 + sizeof("/ACTA Gamma/acta.db") > sizeof path)
        return "./acta.db";

#ifdef _WIN32
    snprintf(path, sizeof path, "%s\\ACTA Gamma\\acta.db", base);
#else
    snprintf(path, sizeof path, "%s/ACTA Gamma/acta.db", base);
#endif
    return path;
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
