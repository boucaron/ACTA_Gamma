#pragma once
#include "baseDao.h"

struct Execution {
    int id = 0;
    int contextId = 0;
    int skillRevisionId = 0;
    int modelRevisionId = 0;
    QString prompt;
    QString rawResponse;
    QString result;
    QString status;
    QString error;
    QString createdAt;
    QString startedAt;
    QString completedAt;
    int parentExecutionId = 0;
};

class ExecutionDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool create(Execution &exec);
    bool update(const Execution &exec);
    bool findById(int id, Execution &out) const;
    QList<Execution> findByStatus(const QString &status) const;
    QList<Execution> findByModelSkillContext(int modelRevId,int skillRevId,int contextId) const;
    bool replay(int executionId, int targetModelRevisionId, Execution &newExec);
};


