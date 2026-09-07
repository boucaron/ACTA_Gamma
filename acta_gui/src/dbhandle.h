#pragma once

#include <QString>

extern "C" {
#include "acta_db.h"
}

// RAII wrapper around the raw acta_db db_t* handle.
//
//  - Non-copyable, movable.
//  - Ownership: the wrapper owns the handle from open() until close()
//    or destruction.
//  - Shutdown (close() / destructor): attempts acta_db_close() (strict
//    close, best-effort ROLLBACK) and falls back to
//    acta_db_force_close() if the strict close fails (e.g. SQLITE_BUSY).
//
// Error surfacing:
//  - open() failures: fill errorCode / errorMessage from
//    acta_db_strerror(). Note: when open() fails there is no handle,
//    so acta_db_last_error() cannot be consulted for it.
//  - Live errors: call lastError() (wraps acta_db_last_error()) while
//    the handle is valid.
class DbHandle {
public:
    DbHandle() = default;
    ~DbHandle();

    DbHandle(const DbHandle&) = delete;
    DbHandle &operator=(const DbHandle&) = delete;
    DbHandle(DbHandle &&other) noexcept;
    DbHandle &operator=(DbHandle &&other) noexcept;

    // Open the database at `path` (default: ACTA_DB_OPEN_EXISTING).
    // Returns true on success. On failure returns false and, if
    // non-NULL, sets *errorCode to the acta_db error code and
    // *errorMessage to acta_db_strerror(code).
    bool open(const QString &path,
              int mode = ACTA_DB_OPEN_EXISTING,
              QString *errorMessage = nullptr,
              int *errorCode = nullptr);

    // Explicit close (strict close via acta_db_close, with
    // force_close fallback). Idempotent; safe to call when invalid.
    // Returns the acta_db_close result (ACTA_DB_ERR_INVALID if no
    // handle was open).
    int close();

    bool valid() const { return m_db != nullptr; }
    db_t *handle() const { return m_db; }

    // Last SQLite error message for this connection (nullptr when the
    // handle is invalid).
    const char *lastError() const;

private:
    // Release a raw handle: acta_db_close(), force_close on failure.
    static void release(db_t *db);

    db_t *m_db = nullptr;
};
