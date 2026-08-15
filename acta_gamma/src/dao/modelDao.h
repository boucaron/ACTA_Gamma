#pragma once
#include "baseDao.h"

struct Model {
    int id = 0;
    int folderId = 0;
    QString name;
    QString description;
    QString backend;
    QString baseUrl;
    QString model;
    QString configuration;
    int currentRevision = 0;
    QString createdAt;
    QString updatedAt;
    QString deletedAt;
};

class ModelDao : public BaseDao {
public:
    using BaseDao::BaseDao;

    bool create(Model &model);
    bool update(const Model &model);
    bool remove(int id);
    bool findById(int id, Model &out) const;
    QList<Model> findAll() const;
    QList<Model> findByFolder(int folderId) const;
};


