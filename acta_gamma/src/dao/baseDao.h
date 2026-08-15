#pragma once
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

class BaseDao {
public:
    explicit BaseDao(QSqlDatabase &db) : m_db(db) {}
    virtual ~BaseDao() = default;

protected:
    const QSqlDatabase &database() const noexcept { return m_db; }
    QSqlDatabase &database() noexcept { return m_db; }
    QString lastError() const;

private:
    QSqlDatabase &m_db;
};


