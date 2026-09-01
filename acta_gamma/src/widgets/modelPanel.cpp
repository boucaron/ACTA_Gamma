#include "modelPanel.h"

#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHash>
#include <QMenu>

#include <functional>

#include "modelDialog.h"

namespace {
const int RoleFolderId        = Qt::UserRole;
const int RoleModelId         = Qt::UserRole + 1;
const int RoleIsDeleted       = Qt::UserRole + 2; // models only
const int RoleFolderIsDeleted = Qt::UserRole + 3; // folders only
} // namespace

ModelPanel::ModelPanel(db_t *db, QWidget *parent) : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Model"));

    tree = new QTreeWidget;
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    lay->addWidget(tree);

    m_deletedIcon = tree->style()->standardIcon(QStyle::SP_TrashIcon);
    m_folderIcon  = tree->style()->standardIcon(QStyle::SP_DirIcon);

    showDeletedCheck = new QCheckBox("Show deleted items");
    lay->addWidget(showDeletedCheck);

    auto *actionRow = new QHBoxLayout;
    newBtn = new QPushButton("New");
    showBtn = new QPushButton("Show");
    editBtn = new QPushButton("Edit Model");
    actionRow->addWidget(newBtn);
    actionRow->addWidget(showBtn);
    actionRow->addWidget(editBtn);
    actionRow->addStretch();
    lay->addLayout(actionRow);

    auto *btnRow = new QHBoxLayout;
    deleteBtn = new QPushButton("Delete");
    restoreBtn = new QPushButton("Restore");
    btnRow->addWidget(deleteBtn);
    btnRow->addWidget(restoreBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    auto *folderRow = new QHBoxLayout;
    newFolderBtn = new QPushButton("New Folder");
    renameFolderBtn = new QPushButton("Rename Folder");
    deleteFolderBtn = new QPushButton("Delete Folder");
    restoreFolderBtn = new QPushButton("Restore Folder");
    folderRow->addWidget(newFolderBtn);
    folderRow->addWidget(renameFolderBtn);
    folderRow->addWidget(deleteFolderBtn);
    folderRow->addWidget(restoreFolderBtn);
    folderRow->addStretch();
    lay->addLayout(folderRow);

    connect(showDeletedCheck, &QCheckBox::toggled, this, [this](bool) {
        reload();
    });
    connect(newBtn, &QPushButton::clicked, this, &ModelPanel::onNewBtnClicked);
    connect(showBtn, &QPushButton::clicked, this, &ModelPanel::onShowBtnClicked);
    connect(editBtn, &QPushButton::clicked, this, &ModelPanel::onEditBtnClicked);
    connect(deleteBtn, &QPushButton::clicked, this,
            &ModelPanel::onModelDeleteClicked);
    connect(restoreBtn, &QPushButton::clicked, this,
            &ModelPanel::onModelRestoreClicked);
    connect(newFolderBtn, &QPushButton::clicked, this,
            &ModelPanel::onNewFolderBtnClicked);
    connect(renameFolderBtn, &QPushButton::clicked, this,
            &ModelPanel::onRenameFolderBtnClicked);
    connect(deleteFolderBtn, &QPushButton::clicked, this,
            &ModelPanel::onDeleteFolderBtnClicked);
    connect(restoreFolderBtn, &QPushButton::clicked, this,
            &ModelPanel::onRestoreFolderBtnClicked);
    connect(tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *, QTreeWidgetItem *) {
                updateButtonStates();
            });
    connect(tree, &QTreeWidget::customContextMenuRequested, this,
            &ModelPanel::onListContextMenu);

    tree->setContextMenuPolicy(Qt::CustomContextMenu);

    reload();
}

void ModelPanel::updateButtonStates()
{
    const auto *cur = tree->currentItem();
    const bool hasModel =
        cur != nullptr && cur->data(0, RoleModelId).toInt() != 0;
    const bool isModelDeleted =
        hasModel && cur->data(0, RoleIsDeleted).toBool();
    const bool hasFolder =
        cur != nullptr && cur->data(0, RoleFolderId).toInt() != 0;
    const bool isFolderDeleted =
        hasFolder && cur->data(0, RoleFolderIsDeleted).toBool();

    showBtn->setEnabled(hasModel);
    editBtn->setEnabled(hasModel && !isModelDeleted);
    deleteBtn->setEnabled(hasModel && !isModelDeleted);
    restoreBtn->setEnabled(hasModel && isModelDeleted);

    // "New Folder" is always usable: it targets the selected folder,
    // or root when a model (or nothing) is selected.
    renameFolderBtn->setEnabled(hasFolder && !isFolderDeleted);
    deleteFolderBtn->setEnabled(hasFolder && !isFolderDeleted);
    restoreFolderBtn->setEnabled(hasFolder && isFolderDeleted);
}

void ModelPanel::reload()
{
    // Preserve the current selection across the rebuild.
    const auto *cur = tree->currentItem();
    const int keepFolder = cur ? cur->data(0, RoleFolderId).toInt() : 0;
    const int keepModel  = cur ? cur->data(0, RoleModelId).toInt() : 0;

    tree->clear();
    if (!m_db)
        return; // db open failed at startup; MainWindow surfaces the reason.

    // Folder skeleton: all folders in one call, nested via parent_id
    // (0 = root). list_all is ordered by id, which is not guaranteed to be
    // parent-before-child, so build the child->parent map first and create
    // each item directly under its parent (QTreeWidgetItem cannot be
    // re-parented once created).
    // Folders whose parent is not in the set stay at the top level.
    // With "Show deleted items" checked, soft-deleted folders are included
    // (and marked) so they can be restored.
    const bool showDeleted =
        showDeletedCheck != nullptr && showDeletedCheck->isChecked();
    int nFolders = 0;
    int err = ACTA_DB_OK;
    model_folder_t **folders = showDeleted
        ? acta_db_model_folder_list_all_with_deleted(m_db, 0, 0, &nFolders,
                                                      &err)
        : acta_db_model_folder_list_all(m_db, 0, 0, &nFolders, &err);
    if (!folders) {
        if (err != ACTA_DB_OK)
            qWarning("model folder lister failed: %s",
                     acta_db_strerror(err));
        return;
    }

    QHash<int, QString> names;
    QHash<int, bool> deleted;
    QHash<int, QList<int>> children;
    QSet<int> ids;
    for (int i = 0; i < nFolders; ++i) {
        names.insert(folders[i]->id, QString::fromUtf8(folders[i]->name));
        deleted.insert(folders[i]->id,
                       folders[i]->deleted_at && folders[i]->deleted_at[0]);
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
        const bool folderDeleted = deleted.value(id, false);
        item->setData(0, RoleFolderIsDeleted, folderDeleted);
        item->setIcon(0, folderDeleted ? m_deletedIcon : m_folderIcon);
        if (folderDeleted)
            item->setForeground(0, QColor(Qt::gray));
        folderItems.insert(id, item);
        const QList<int> kids = children.value(id);
        for (int kid : kids)
            addFolder(kid, item);
    };
    for (int i = 0; i < nFolders; ++i) {
        if (folders[i]->parent_id == 0 || !ids.contains(folders[i]->parent_id))
            addFolder(folders[i]->id, nullptr);
    }

    // Models: root level (folder_id 0), then one row per folder.
    addModels(nullptr, 0);
    for (auto it = folderItems.cbegin(); it != folderItems.cend(); ++it)
        addModels(it.value(), it.key());

    acta_db_model_folder_list_free(folders, nFolders);

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        tree->topLevelItem(i)->setExpanded(true);

    // Re-select the previously current item (folder first: a deleted
    // folder can vanish from a live-only rebuild).
    QTreeWidgetItem *keep = nullptr;
    if (keepFolder != 0)
        keep = findItemByRole(RoleFolderId, keepFolder);
    else if (keepModel != 0)
        keep = findItemByRole(RoleModelId, keepModel);
    if (keep)
        tree->setCurrentItem(keep);

    updateButtonStates();
}

void ModelPanel::addModels(QTreeWidgetItem *parent, int folderId)
{
    int n = 0;
    int err = ACTA_DB_OK;
    const bool showDeleted =
        showDeletedCheck != nullptr && showDeletedCheck->isChecked();
    model_t **models = showDeleted
        ? acta_db_model_list_in_folder_with_deleted(m_db, folderId, 0, 0, &n,
                                                    &err)
        : acta_db_model_list_in_folder(m_db, folderId, 0, 0, &n, &err);
    if (!models) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_list_in_folder(%d) failed: %s", folderId,
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = parent
            ? new QTreeWidgetItem(parent, {QString::fromUtf8(models[i]->name)})
            : new QTreeWidgetItem(tree, {QString::fromUtf8(models[i]->name)});
        item->setData(0, RoleModelId, models[i]->id);
        const bool isDeleted =
            models[i]->deleted_at && models[i]->deleted_at[0];
        item->setData(0, RoleIsDeleted, isDeleted);
        if (isDeleted) {
            item->setIcon(0, m_deletedIcon);
            item->setForeground(0, QColor(Qt::gray));
        }
    }
    acta_db_model_list_free(models, n);
}

int ModelPanel::selectedModelId() const
{
    const auto *cur = tree->currentItem();
    return cur ? cur->data(0, RoleModelId).toInt() : 0;
}

int ModelPanel::selectedFolderId() const
{
    const auto *cur = tree->currentItem();
    return cur ? cur->data(0, RoleFolderId).toInt() : 0;
}

QTreeWidgetItem *ModelPanel::findItemByRole(int role, int id) const
{
    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> search =
        [&](QTreeWidgetItem *item) -> QTreeWidgetItem * {
            if (item->data(0, role).toInt() == id)
                return item;
            for (int i = 0; i < item->childCount(); ++i) {
                QTreeWidgetItem *found = search(item->child(i));
                if (found)
                    return found;
            }
            return nullptr;
        };
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *found = search(tree->topLevelItem(i));
        if (found)
            return found;
    }
    return nullptr;
}

void ModelPanel::onNewBtnClicked()
{
    if (!m_db)
        return;

    // New model goes into the selected folder when one is selected,
    // otherwise at the root level.
    ModelDialog dlg(this);
    dlg.newModel(m_db, selectedFolderId());
    dlg.exec();
    reload();
}

void ModelPanel::onShowBtnClicked()
{
    const int modelId = selectedModelId();
    if (modelId == 0 || !m_db)
        return;

    ModelDialog dlg(this);
    dlg.showModel(m_db, modelId);
    dlg.exec();
}

void ModelPanel::onEditBtnClicked()
{
    const int modelId = selectedModelId();
    if (modelId == 0 || !m_db)
        return;

    ModelDialog dlg(this);
    dlg.editModel(m_db, modelId);
    dlg.exec();
    reload();
}

void ModelPanel::onModelDeleteClicked()
{
    const int modelId = selectedModelId();
    if (!m_db || modelId == 0)
        return;
    if (acta_db_model_soft_delete(m_db, modelId) == ACTA_DB_OK)
        reload();
}

void ModelPanel::onModelRestoreClicked()
{
    const int modelId = selectedModelId();
    if (!m_db || modelId == 0)
        return;
    if (acta_db_model_restore(m_db, modelId) == ACTA_DB_OK)
        reload();
}

void ModelPanel::onListContextMenu(const QPoint &pos)
{
    const auto *item = tree->itemAt(pos);
    if (!item)
        return;
    // A right-click selects the row, so the handlers operate on it
    // exactly as the button row does.
    tree->setCurrentItem(const_cast<QTreeWidgetItem *>(item));

    const bool hasModel = item->data(0, RoleModelId).toInt() != 0;
    const bool isModelDeleted =
        hasModel && item->data(0, RoleIsDeleted).toBool();
    const bool hasFolder = item->data(0, RoleFolderId).toInt() != 0;
    const bool isFolderDeleted =
        hasFolder && item->data(0, RoleFolderIsDeleted).toBool();

    QMenu menu(this);
    auto *aNew = menu.addAction(tr("New"), this, &ModelPanel::onNewBtnClicked);
    auto *aShow = menu.addAction(tr("Show"), this, &ModelPanel::onShowBtnClicked);
    auto *aEdit = menu.addAction(tr("Edit Model"), this, &ModelPanel::onEditBtnClicked);
    auto *aDelete = menu.addAction(tr("Delete"), this,
                                  &ModelPanel::onModelDeleteClicked);
    auto *aRestore = menu.addAction(tr("Restore"), this,
                                   &ModelPanel::onModelRestoreClicked);
    menu.addSeparator();
    auto *aNewFolder = menu.addAction(tr("New Folder"), this,
                                     &ModelPanel::onNewFolderBtnClicked);
    auto *aRenameFolder = menu.addAction(tr("Rename Folder"), this,
                                         &ModelPanel::onRenameFolderBtnClicked);
    auto *aDeleteFolder = menu.addAction(tr("Delete Folder"), this,
                                         &ModelPanel::onDeleteFolderBtnClicked);
    auto *aRestoreFolder = menu.addAction(tr("Restore Folder"), this,
                                          &ModelPanel::onRestoreFolderBtnClicked);

    // Same rules as updateButtonStates(): "New" / "New Folder" are
    // always usable, the rest depend on what the row is.
    aNew->setEnabled(m_db != nullptr);
    aShow->setEnabled(hasModel);
    aEdit->setEnabled(hasModel && !isModelDeleted);
    aDelete->setEnabled(hasModel && !isModelDeleted);
    aRestore->setEnabled(hasModel && isModelDeleted);
    aNewFolder->setEnabled(m_db != nullptr);
    aRenameFolder->setEnabled(hasFolder && !isFolderDeleted);
    aDeleteFolder->setEnabled(hasFolder && !isFolderDeleted);
    aRestoreFolder->setEnabled(hasFolder && isFolderDeleted);

    menu.exec(tree->viewport()->mapToGlobal(pos));
}

void ModelPanel::onNewFolderBtnClicked()
{
    if (!m_db)
        return;

    // New folder goes into the selected folder when one is selected,
    // otherwise at the root level.
    const int parentId = selectedFolderId();
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal,
        QString(), &ok);
    if (!ok)
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;

    int newId = 0;
    const int rc =
        acta_db_model_folder_create(m_db, trimmed.toUtf8().constData(),
                                    parentId, &newId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, tr("Model"),
            tr("Could not create folder: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }

    reload();
    if (QTreeWidgetItem *item = findItemByRole(RoleFolderId, newId))
        tree->setCurrentItem(item);
}

void ModelPanel::onRenameFolderBtnClicked()
{
    const int folderId = selectedFolderId();
    if (!m_db || folderId == 0)
        return;

    const auto *cur = tree->currentItem();
    const QString currentName = cur ? cur->text(0) : QString();
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Folder"), tr("Folder name:"), QLineEdit::Normal,
        currentName, &ok);
    if (!ok)
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;

    const int rc = acta_db_model_folder_rename(
        m_db, folderId, trimmed.toUtf8().constData());
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, tr("Model"),
            tr("Could not rename folder: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }

    reload(); // reload() restores the selection to the renamed folder
}

void ModelPanel::onDeleteFolderBtnClicked()
{
    const int folderId = selectedFolderId();
    if (!m_db || folderId == 0)
        return;

    const auto *cur = tree->currentItem();
    const QString name = cur ? cur->text(0) : QString();
    if (QMessageBox::question(
            this, tr("Delete Folder"),
            tr("Delete folder '%1'? Its models stay in the database but "
               "are hidden until the folder is restored.")
                .arg(name))
        != QMessageBox::Yes)
        return;

    const int rc = acta_db_model_folder_soft_delete(m_db, folderId);
    if (rc != ACTA_DB_OK) {
        QString msg;
        switch (rc) {
        case ACTA_DB_ERR_INVALID:
            // The folder still has live child folders or live models.
            msg = tr("Folder still has live child folders or models. "
                      "Delete or move them first.");
            break;
        case ACTA_DB_ERR_NOT_FOUND:
            msg = tr("Folder not found.");
            break;
        default:
            msg = tr("Could not delete folder: %1")
                    .arg(QString::fromUtf8(acta_db_strerror(rc)));
            break;
        }
        QMessageBox::warning(this, tr("Model"), msg);
        return;
    }

    reload();
}

void ModelPanel::onRestoreFolderBtnClicked()
{
    const int folderId = selectedFolderId();
    if (!m_db || folderId == 0)
        return;

    const int rc = acta_db_model_folder_restore(m_db, folderId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, tr("Model"),
            tr("Could not restore folder: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }

    reload();
}
