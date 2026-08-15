#include "baseDao.h"

QString BaseDao::lastError() const
{
    return m_db.lastError().text();
}
