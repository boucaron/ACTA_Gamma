#pragma once
#include "baseDao.h"


struct ModelFolder {
    int id = 0;
    QString name;
    int parentId = 0;
    QString createdAt;
    QString updatedAt;
    QString deletedAt;
};

class ModelFolderDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool create(ModelFolder &folder);
    bool update(const ModelFolder &folder);
    bool remove(int id);
    bool findById(int id, ModelFolder &out) const;
    QList<ModelFolder> findAll() const;
    QList<ModelFolder> findByParent(int parentId) const;
};

