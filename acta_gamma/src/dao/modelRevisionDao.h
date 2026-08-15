#pragma once
#include "baseDao.h"


struct ModelRevision {
    int id = 0;
    int modelId = 0;
    int revision = 0;
    int folderId = 0;
    QString name;
    QString description;
    QString backend;
    QString baseUrl;
    QString model;
    QString configuration;
    QString createdAt;
    QString updatedAt;
    QString deletedAt;
};

class ModelRevisionDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool findById(int id, ModelRevision &out) const;
    bool findByModelRevision(int modelId, int revision, ModelRevision &out) const;
    QList<ModelRevision> findByModel(int modelId) const;
    ModelRevision latest(int modelId) const;
};

