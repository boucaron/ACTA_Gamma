#pragma once
#include "baseDao.h"


struct SkillRevision {
    int id = 0;
    int skillId = 0;
    int folderId = 0;
    QString name;
    QString description;
    int revision = 0;
    QString promptTemplate;
    QString outputSchema;
    QString createdAt;
    QString updatedAt;
    QString deletedAt;
};

class SkillRevisionDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool findById(int id, SkillRevision &out) const;
    bool findBySkillRevision(int skillId, int revision, SkillRevision &out) const;
    QList<SkillRevision> findBySkill(int skillId) const;
    SkillRevision latest(int skillId) const;
};

