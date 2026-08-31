#include "skillPanel.h"

#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHash>

#include <functional>

#include "skillDialog.h"

namespace {
const int RoleFolderId = Qt::UserRole;
const int RoleSkillId  = Qt::UserRole + 1;
} // namespace

SkillPanel::SkillPanel(db_t *db, QWidget *parent) : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Skill"));

    tree = new QTreeWidget;
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    lay->addWidget(tree);

    loadBtn = new QPushButton("Load Skill");
    lay->addWidget(loadBtn);

    connect(loadBtn, &QPushButton::clicked, this, &SkillPanel::onLoadBtnClicked);
    connect(tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                loadBtn->setEnabled(
                    cur != nullptr && cur->data(0, RoleSkillId).toInt() != 0);
            });

    reload();
}

void SkillPanel::reload()
{
    tree->clear();
    if (!m_db)
        return; // db open failed at startup; MainWindow surfaces the reason.

    // Folder skeleton: all live folders in one call, nested via parent_id
    // (0 = root). list_all is ordered by id, which is not guaranteed to be
    // parent-before-child, so build the child->parent map first and create
    // each item directly under its parent (QTreeWidgetItem cannot be
    // re-parented once created).
    // Folders whose parent is not in the live set stay at the top level.
    int nFolders = 0;
    int err = ACTA_DB_OK;
    skill_folder_t **folders =
        acta_db_skill_folder_list_all(m_db, 0, 0, &nFolders, &err);
    if (!folders) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_folder_list_all failed: %s",
                     acta_db_strerror(err));
        return;
    }

    QHash<int, QString> names;
    QHash<int, QList<int>> children;
    QSet<int> ids;
    for (int i = 0; i < nFolders; ++i) {
        names.insert(folders[i]->id, QString::fromUtf8(folders[i]->name));
        ids.insert(folders[i]->id);
        if (folders[i]->parent_id != 0)
            children[folders[i]->parent_id].append(folders[i]->id);
    }

    QHash<int, QTreeWidgetItem *> folderItems;
    std::function<void(int, QTreeWidgetItem *)> addFolder;
    addFolder = [&](int id, QTreeWidgetItem *parent) {
        if (folderItems.contains(id))
            return; // defensive: data with a parent cycle
        auto *item =
            parent ? new QTreeWidgetItem(parent, {names.value(id)})
                   : new QTreeWidgetItem(tree, {names.value(id)});
        item->setData(0, RoleFolderId, id);
        folderItems.insert(id, item);
        const QList<int> kids = children.value(id);
        for (int kid : kids)
            addFolder(kid, item);
    };
    for (int i = 0; i < nFolders; ++i) {
        if (folders[i]->parent_id == 0 || !ids.contains(folders[i]->parent_id))
            addFolder(folders[i]->id, nullptr);
    }

    // Skills: root level (folder_id 0), then one row per folder.
    addSkills(nullptr, 0);
    for (auto it = folderItems.cbegin(); it != folderItems.cend(); ++it)
        addSkills(it.value(), it.key());

    acta_db_skill_folder_list_free(folders, nFolders);

    for (int i = 0; i < tree->topLevelCount(); ++i)
        tree->topLevelItem(i)->setExpanded(true);
}

void SkillPanel::addSkills(QTreeWidgetItem *parent, int folderId)
{
    int n = 0;
    int err = ACTA_DB_OK;
    skill_t **skills =
        acta_db_skill_list_in_folder(m_db, folderId, 0, 0, &n, &err);
    if (!skills) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_list_in_folder(%d) failed: %s", folderId,
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = parent
            ? new QTreeWidgetItem(parent, {QString::fromUtf8(skills[i]->name)})
            : new QTreeWidgetItem(tree, {QString::fromUtf8(skills[i]->name)});
        item->setData(0, RoleSkillId, skills[i]->id);
    }
    acta_db_skill_list_free(skills, n);
}

int SkillPanel::selectedSkillId() const
{
    const auto *cur = tree->currentItem();
    return cur ? cur->data(0, RoleSkillId).toInt() : 0;
}

void SkillPanel::onLoadBtnClicked()
{
    SkillDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // TODO
    }
}
