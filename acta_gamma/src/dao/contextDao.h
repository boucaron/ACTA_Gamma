#pragma once
#include "baseDao.h"

struct Context {
    int id = 0;
    QString type;
    QString content;
    QString contentHash;
    QString metadata;
    QString createdAt;
};

class ContextDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool create(Context &ctx);
    bool findById(int id, Context &out) const;
    bool findByHash(const QString &hash, Context &out) const;
    QList<Context> findByType(const QString &type) const;
};

