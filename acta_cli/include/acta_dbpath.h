/*
 * acta_dbpath.h — DB path resolution shared by acta_cli and acta_runner.
 *
 * Resolution order (single source of truth: docs/cli_spec.md, "DB file"):
 *   1. the --db path passed by the caller (used as-is)
 *   2. $ACTA_DB, if set and non-empty
 *   3. the config file's "db" — ACTA_Gamma.conf in the same app-data
 *      directory as the default DB file (docs/plans/acta-config-file.md,
 *      work item 3): a missing/unreadable file is skipped, a
 *      readable-but-malformed one is a hard error, an empty value is
 *      treated as absent
 *   4. the platform app-data default — the same file the GUI uses:
 *        Windows : %APPDATA%\ACTA_Gamma\acta.db
 *        POSIX   : $XDG_DATA_HOME/ACTA_Gamma/acta.db
 *                  (else $HOME/.local/share/ACTA_Gamma/acta.db)
 *   5. ./acta.db — last resort only, when the platform base directory
 *      is unresolvable (no %APPDATA% / $XDG_DATA_HOME / $HOME).
 *
 * The path deliberately carries NO organization-name segment: the GUI
 * builds the same file explicitly (MainWindow::defaultDbPath) rather
 * than via QStandardPaths::AppDataLocation, which would append the org.
 *
 * NOTE (keep in lockstep): this header and the matching acta_dbpath.c
 * are duplicated verbatim — acta_cli/{include,src} and
 * acta_runner/{include,src}. Edit all four files together.
 */
#ifndef ACTA_DBPATH_H
#define ACTA_DBPATH_H

#include <stddef.h>

/* Return the DB path: `flag` if non-NULL and non-empty, else $ACTA_DB,
 * else the config file's "db" (missing/unreadable file skipped;
 * readable-but-malformed file is a hard error), else the platform
 * app-data default, else "./acta.db".
 * On hard error returns NULL and sets *err_msg to a malloc'd one-line
 * diagnostic the caller must free (err_msg may be NULL).
 * Otherwise the returned pointer is valid until the next call (static
 * buffer, or the caller's own `flag`). */
const char *acta_db_resolve_db_path(const char *flag, char **err_msg);

/* The platform app-data default, or "./acta.db" as a last resort when
 * the platform base directory cannot be resolved.  Valid until the next
 * call (static buffer). */
const char *acta_db_default_db_path(void);

/* If ./acta.db exists in the current working directory, write a short
 * hint into hint (at most hint_size bytes) and return 1; otherwise
 * return 0. Only to be used when the *default* path was resolved (no
 * --db, no $ACTA_DB, no config-file "db") and that default file cannot
 * be opened. */
int acta_db_legacy_db_hint(char *hint, size_t hint_size);

#endif /* ACTA_DBPATH_H */
