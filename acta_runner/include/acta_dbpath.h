/*
 * acta_dbpath.h — DB path resolution shared by acta_cli and acta_runner.
 *
 * Resolution order (single source of truth: docs/cli_spec.md, "DB file"):
 *   1. the --db path passed by the caller (used as-is)
 *   2. $ACTA_DB, if set and non-empty
 *   3. the platform app-data default — the same file the GUI uses:
 *        Windows : %APPDATA%\ACTA Gamma\acta.db
 *        POSIX   : $XDG_DATA_HOME/ACTA Gamma/acta.db
 *                  (else $HOME/.local/share/ACTA Gamma/acta.db)
 *   4. ./acta.db — last resort only, when the platform base directory
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
 * else the platform app-data default, else "./acta.db".
 * The returned pointer is valid until the next call (static buffer, or
 * the caller's own `flag`). */
const char *acta_db_resolve_db_path(const char *flag);

/* If ./acta.db exists in the current working directory, write a short
 * hint into hint (at most hint_size bytes) and return 1; otherwise
 * return 0. Only to be used when the *default* path was resolved (no
 * --db, no $ACTA_DB) and that default file cannot be opened. */
int acta_db_legacy_db_hint(char *hint, size_t hint_size);

#endif /* ACTA_DBPATH_H */
