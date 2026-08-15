#pragma once
#include "baseDao.h"

struct Skill {
    int id = 0;
    int folderId = 0;
    QString name;
    QString description;
    QString promptTemplate;
    QString outputSchema;
    int currentRevision = 0;
    QString createdAt;
    QString updatedAt;
    QString deletedAt;
};

class SkillDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool create(Skill &skill);
    bool update(const Skill &skill);
    bool remove(int id);
    bool findById(int id, Skill &out) const;
    QList<Skill> findByFolder(int folderId) const;
};


