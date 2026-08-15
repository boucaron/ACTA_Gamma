#pragma once
#include "baseDao.h"

struct ExecutionLog {
    int id = 0;
    int executionId = 0;
    QString level;
    QString event;
    QString message;
    QString metadata;
    QString createdAt;
};

class ExecutionLogDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool append(const ExecutionLog &log);
    QList<ExecutionLog> findByExecution(int executionId) const;
    QList<ExecutionLog> findByEvent(const QString &event) const;
};


