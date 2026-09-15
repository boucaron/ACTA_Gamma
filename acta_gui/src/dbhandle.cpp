#include "dbhandle.h"

#include <QtGlobal>

namespace {

// Release a raw handle: strict close first, force close as the
// guaranteed teardown path (per acta_db db.h: force_close is intended
// for atexit/shutdown where a guaranteed release is required).
void releaseHandle(db_t *db)
{
    if (db == nullptr)
        return;

    const int r = acta_db_close(db);
    if (r != ACTA_DB_OK) {
        qWarning("acta_db_close failed (%s, code %d); force-closing",
                 acta_db_strerror(r), r);
        acta_db_force_close(db);
    }
}

} // namespace

DbHandle::DbHandle(DbHandle &&other) noexcept
{
    m_db = other.m_db;
    other.m_db = nullptr;
}

DbHandle &DbHandle::operator=(DbHandle &&other) noexcept
{
    if (this != &other) {
        releaseHandle(m_db);
        m_db = other.m_db;
        other.m_db = nullptr;
    }
    return *this;
}

DbHandle::~DbHandle()
{
    releaseHandle(m_db);
    m_db = nullptr;
}

bool DbHandle::open(const QString &path,
                    int mode,
                    QString *errorMessage,
                    int *errorCode)
{
    if (valid())
        releaseHandle(m_db);

    int err = ACTA_DB_OK;
    const QByteArray pathUtf8 = path.toUtf8();
    db_t *db = acta_db_open(pathUtf8.constData(), &err, mode);
    if (db == nullptr || err != ACTA_DB_OK) {
        if (errorCode)
            *errorCode = err;
        if (errorMessage)
            *errorMessage = QString::fromUtf8(acta_db_strerror(err));
        m_db = nullptr;
        return false;
    }

    /* Informational pragma note after a successful open (e.g. WAL not
     * supported by the backend). Not an error. */
    if (const char *note = acta_db_last_error(db))
        qWarning("db opened with degraded pragma state: %s", note);

    m_db = db;
    return true;
}

int DbHandle::close()
{
    if (!valid())
        return ACTA_DB_ERR_INVALID;

    db_t *db = m_db;
    m_db = nullptr;
    const int r = acta_db_close(db);
    if (r != ACTA_DB_OK) {
        qWarning("acta_db_close failed (%s, code %d); force-closing",
                 acta_db_strerror(r), r);
        acta_db_force_close(db);
    }
    return r;
}

const char *DbHandle::lastError() const
{
    return valid() ? acta_db_last_error(m_db) : nullptr;
}
