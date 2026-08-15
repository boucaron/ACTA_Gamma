#pragma once
#include "baseDao.h"

struct SkillFolder {
    int id = 0;
    QString name;
    int parentId = 0;
    QString createdAt;
    QString updatedAt;
    QString deletedAt;
};

class SkillFolderDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool create(SkillFolder &folder);
    bool update(const SkillFolder &folder);
    bool remove(int id);
    bool findById(int id, SkillFolder &out) const;
    QList<SkillFolder> findByParent(int parentId) const;
};

