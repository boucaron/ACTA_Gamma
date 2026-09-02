#include "folderTreePanel.h"

#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QEvent>
#include <QKeyEvent>
#include <QSet>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHash>
#include <QMenu>

#include <functional>

namespace {
const int RoleFolderId        = Qt::UserRole;
const int RoleEntityId        = Qt::UserRole + 1;
const int RoleIsDeleted       = Qt::UserRole + 2; // entities only
const int RoleFolderIsDeleted = Qt::UserRole + 3; // folders only
} // namespace

FolderTreePanel::FolderTreePanel(FolderTreeDao dao, QWidget *parent)
    : QWidget(parent), m_dao(std::move(dao))
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(m_dao.entityTitle));

    tree = new QTreeWidget;
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    lay->addWidget(tree);

    m_deletedIcon = tree->style()->standardIcon(QStyle::SP_TrashIcon);
    m_folderIcon  = tree->style()->standardIcon(QStyle::SP_DirIcon);

    showDeletedCheck = new QCheckBox("Show deleted items");
    lay->addWidget(showDeletedCheck);

    // "&" marks each button's accelerator (Alt+letter) (UR #39).
    auto *actionRow = new QHBoxLayout;
    newBtn = new QPushButton("&New");
    showBtn = new QPushButton("S&how");
    editBtn = new QPushButton(QString("E&dit ") + m_dao.entityTitle);
    actionRow->addWidget(newBtn);
    actionRow->addWidget(showBtn);
    actionRow->addWidget(editBtn);
    actionRow->addStretch();
    lay->addLayout(actionRow);

    auto *btnRow = new QHBoxLayout;
    deleteBtn = new QPushButton("&Delete");
    restoreBtn = new QPushButton("Res&tore");
    btnRow->addWidget(deleteBtn);
    btnRow->addWidget(restoreBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    auto *folderRow = new QHBoxLayout;
    newFolderBtn = new QPushButton("New F&older");
    renameFolderBtn = new QPushButton("Rename &Folder");
    deleteFolderBtn = new QPushButton("De&lete Folder");
    restoreFolderBtn = new QPushButton("&Restore Folder");
    folderRow->addWidget(newFolderBtn);
    folderRow->addWidget(renameFolderBtn);
    folderRow->addWidget(deleteFolderBtn);
    folderRow->addWidget(restoreFolderBtn);
    folderRow->addStretch();
    lay->addLayout(folderRow);

    connect(showDeletedCheck, &QCheckBox::toggled, this, [this](bool) {
        reload();
    });
    connect(newBtn, &QPushButton::clicked, this, &FolderTreePanel::onNewBtnClicked);
    connect(showBtn, &QPushButton::clicked, this, &FolderTreePanel::onShowBtnClicked);
    connect(editBtn, &QPushButton::clicked, this, &FolderTreePanel::onEditBtnClicked);
    connect(deleteBtn, &QPushButton::clicked, this,
            &FolderTreePanel::onEntityDeleteClicked);
    connect(restoreBtn, &QPushButton::clicked, this,
            &FolderTreePanel::onEntityRestoreClicked);
    connect(newFolderBtn, &QPushButton::clicked, this,
            &FolderTreePanel::onNewFolderBtnClicked);
    connect(renameFolderBtn, &QPushButton::clicked, this,
            &FolderTreePanel::onRenameFolderBtnClicked);
    connect(deleteFolderBtn, &QPushButton::clicked, this,
            &FolderTreePanel::onDeleteFolderBtnClicked);
    connect(restoreFolderBtn, &QPushButton::clicked, this,
            &FolderTreePanel::onRestoreFolderBtnClicked);
    connect(tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *, QTreeWidgetItem *) {
                updateButtonStates();
            });
    connect(tree, &QTreeWidget::customContextMenuRequested, this,
            &FolderTreePanel::onListContextMenu);

    tree->setContextMenuPolicy(Qt::CustomContextMenu);

    // Keyboard accelerators (UR #39), handled in eventFilter() while the
    // tree (or its viewport) has focus. Installed on both because either
    // widget can be the focus target after a click; dialogs are separate
    // widgets, so this never fires inside them.
    tree->installEventFilter(this);
    tree->viewport()->installEventFilter(this);

    reload();
}

void FolderTreePanel::updateButtonStates()
{
    const auto *cur = tree->currentItem();
    const bool hasEntity =
        cur != nullptr && cur->data(0, RoleEntityId).toInt() != 0;
    const bool isEntityDeleted =
        hasEntity && cur->data(0, RoleIsDeleted).toBool();
    const bool hasFolder =
        cur != nullptr && cur->data(0, RoleFolderId).toInt() != 0;
    const bool isFolderDeleted =
        hasFolder && cur->data(0, RoleFolderIsDeleted).toBool();

    showBtn->setEnabled(hasEntity);
    editBtn->setEnabled(hasEntity && !isEntityDeleted);
    deleteBtn->setEnabled(hasEntity && !isEntityDeleted);
    restoreBtn->setEnabled(hasEntity && isEntityDeleted);

    // "New Folder" is always usable: it targets the selected folder,
    // or root when an entity (or nothing) is selected.
    renameFolderBtn->setEnabled(hasFolder && !isFolderDeleted);
    deleteFolderBtn->setEnabled(hasFolder && !isFolderDeleted);
    restoreFolderBtn->setEnabled(hasFolder && isFolderDeleted);
}

void FolderTreePanel::reload()
{
    // Preserve the current selection across the rebuild.
    const auto *cur = tree->currentItem();
    const int keepFolder = cur ? cur->data(0, RoleFolderId).toInt() : 0;
    const int keepEntity  = cur ? cur->data(0, RoleEntityId).toInt() : 0;

    tree->clear();
    if (!m_dao.listFolders)
        return; // db open failed at startup; MainWindow surfaces the reason.

    // Folder skeleton: all folders in one call, nested via parent_id
    // (0 = root). list_all is ordered by id, which is not guaranteed to
    // be parent-before-child, so build the child->parent map first and
    // create each item directly under its parent (QTreeWidgetItem
    // cannot be re-parented once created).
    // Folders whose parent is not in the set stay at the top level.
    // With "Show deleted items" checked, soft-deleted folders are
    // included (and marked) so they can be restored.
    const bool showDeleted =
        showDeletedCheck != nullptr && showDeletedCheck->isChecked();
    const QList<FolderRow> folders = m_dao.listFolders(showDeleted);

    QHash<int, QString> names;
    QHash<int, bool> deleted;
    QHash<int, QList<int>> children;
    QSet<int> ids;
    for (const FolderRow &f : folders) {
        names.insert(f.id, f.name);
        deleted.insert(f.id, !f.deletedAt.isEmpty());
        ids.insert(f.id);
        if (f.parent_id != 0)
            children[f.parent_id].append(f.id);
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
    for (const FolderRow &f : folders) {
        if (f.parent_id == 0 || !ids.contains(f.parent_id))
            addFolder(f.id, nullptr);
    }

    // Entities: root level (folder_id 0), then one row per folder.
    addEntities(nullptr, 0);
    for (auto it = folderItems.cbegin(); it != folderItems.cend(); ++it)
        addEntities(it.value(), it.key());

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        tree->topLevelItem(i)->setExpanded(true);

    // Re-select the previously current item (folder first: a deleted
    // folder can vanish from a live-only rebuild).
    QTreeWidgetItem *keep = nullptr;
    if (keepFolder != 0)
        keep = findItemByRole(RoleFolderId, keepFolder);
    else if (keepEntity != 0)
        keep = findItemByRole(RoleEntityId, keepEntity);
    if (keep)
        tree->setCurrentItem(keep);

    updateButtonStates();
}

void FolderTreePanel::addEntities(QTreeWidgetItem *parent, int folderId)
{
    const QList<FolderRow> entities =
        m_dao.listEntities(folderId,
                            showDeletedCheck != nullptr &&
                                showDeletedCheck->isChecked());

    for (const FolderRow &e : entities) {
        auto *item = parent
            ? new QTreeWidgetItem(parent, {e.name})
            : new QTreeWidgetItem(tree, {e.name});
        item->setData(0, RoleEntityId, e.id);
        const bool isDeleted = !e.deletedAt.isEmpty();
        item->setData(0, RoleIsDeleted, isDeleted);
        if (isDeleted) {
            item->setIcon(0, m_deletedIcon);
            item->setForeground(0, QColor(Qt::gray));
        }
    }
}

int FolderTreePanel::selectedEntityId() const
{
    const auto *cur = tree->currentItem();
    return cur ? cur->data(0, RoleEntityId).toInt() : 0;
}

int FolderTreePanel::selectedFolderId() const
{
    const auto *cur = tree->currentItem();
    return cur ? cur->data(0, RoleFolderId).toInt() : 0;
}

QTreeWidgetItem *FolderTreePanel::findItemByRole(int role, int id) const
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

void FolderTreePanel::onNewBtnClicked()
{
    if (!m_dao.openNew)
        return;

    // New entity goes into the selected folder when one is selected,
    // otherwise at the root level.
    m_dao.openNew(this, selectedFolderId());
    reload();
}

void FolderTreePanel::onShowBtnClicked()
{
    const int entityId = selectedEntityId();
    if (entityId == 0 || !m_dao.openShow)
        return;

    m_dao.openShow(this, entityId);
}

void FolderTreePanel::onEditBtnClicked()
{
    const int entityId = selectedEntityId();
    if (entityId == 0 || !m_dao.openEdit)
        return;

    m_dao.openEdit(this, entityId);
    reload();
}

void FolderTreePanel::onEntityDeleteClicked()
{
    const int entityId = selectedEntityId();
    if (!m_dao.softDeleteEntity || entityId == 0)
        return;

    const auto *cur = tree->currentItem();
    const QString name = cur ? cur->text(0) : QString();
    if (QMessageBox::question(
            this, tr("Delete %1").arg(m_dao.entityTitle),
            tr("Delete %1 '%2'? It stays in the database and can be "
               "restored.")
                .arg(m_dao.entityTitle.toLower())
                .arg(name))
        != QMessageBox::Yes)
        return;

    if (m_dao.softDeleteEntity(entityId) == ACTA_DB_OK)
        reload();
}

void FolderTreePanel::onEntityRestoreClicked()
{
    const int entityId = selectedEntityId();
    if (!m_dao.restoreEntity || entityId == 0)
        return;
    if (m_dao.restoreEntity(entityId) == ACTA_DB_OK) {
        QMessageBox::information(this, m_dao.entityTitle,
                                tr("%1 restored.").arg(m_dao.entityTitle));
        reload();
    }
}

void FolderTreePanel::onListContextMenu(const QPoint &pos)
{
    const auto *item = tree->itemAt(pos);
    if (!item)
        return;
    // A right-click selects the row, so the handlers operate on it
    // exactly as the button row does.
    tree->setCurrentItem(const_cast<QTreeWidgetItem *>(item));

    const bool hasEntity = item->data(0, RoleEntityId).toInt() != 0;
    const bool isEntityDeleted =
        hasEntity && item->data(0, RoleIsDeleted).toBool();
    const bool hasFolder = item->data(0, RoleFolderId).toInt() != 0;
    const bool isFolderDeleted =
        hasFolder && item->data(0, RoleFolderIsDeleted).toBool();

    QMenu menu(this);
    auto *aNew = menu.addAction(tr("New"), this,
                               &FolderTreePanel::onNewBtnClicked);
    auto *aShow = menu.addAction(tr("Show"), this,
                                &FolderTreePanel::onShowBtnClicked);
    auto *aEdit = menu.addAction(tr("Edit %1").arg(m_dao.entityTitle), this,
                                &FolderTreePanel::onEditBtnClicked);
    auto *aDelete = menu.addAction(tr("Delete"), this,
                                  &FolderTreePanel::onEntityDeleteClicked);
    auto *aRestore = menu.addAction(tr("Restore"), this,
                                   &FolderTreePanel::onEntityRestoreClicked);
    menu.addSeparator();
    auto *aNewFolder = menu.addAction(tr("New Folder"), this,
                                     &FolderTreePanel::onNewFolderBtnClicked);
    auto *aRenameFolder = menu.addAction(tr("Rename Folder"), this,
                                         &FolderTreePanel::onRenameFolderBtnClicked);
    auto *aDeleteFolder = menu.addAction(tr("Delete Folder"), this,
                                         &FolderTreePanel::onDeleteFolderBtnClicked);
    auto *aRestoreFolder = menu.addAction(tr("Restore Folder"), this,
                                          &FolderTreePanel::onRestoreFolderBtnClicked);

    // Same rules as updateButtonStates(): "New" / "New Folder" are
    // always usable, the rest depend on what the row is.
    aNew->setEnabled(m_dao.openNew != nullptr);
    aShow->setEnabled(hasEntity);
    aEdit->setEnabled(hasEntity && !isEntityDeleted);
    aDelete->setEnabled(hasEntity && !isEntityDeleted);
    aRestore->setEnabled(hasEntity && isEntityDeleted);
    aNewFolder->setEnabled(m_dao.createFolder != nullptr);
    aRenameFolder->setEnabled(hasFolder && !isFolderDeleted);
    aDeleteFolder->setEnabled(hasFolder && !isFolderDeleted);
    aRestoreFolder->setEnabled(hasFolder && isFolderDeleted);

    menu.exec(tree->viewport()->mapToGlobal(pos));
}

void FolderTreePanel::onNewFolderBtnClicked()
{
    if (!m_dao.createFolder)
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
    const int rc = m_dao.createFolder(parentId, trimmed, &newId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, m_dao.entityTitle,
            tr("Could not create folder: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }

    reload();
    if (QTreeWidgetItem *item = findItemByRole(RoleFolderId, newId))
        tree->setCurrentItem(item);
}

void FolderTreePanel::onRenameFolderBtnClicked()
{
    const int folderId = selectedFolderId();
    if (!m_dao.renameFolder || folderId == 0)
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

    const int rc = m_dao.renameFolder(folderId, trimmed);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, m_dao.entityTitle,
            tr("Could not rename folder: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }

    reload(); // reload() restores the selection to the renamed folder
}

void FolderTreePanel::onDeleteFolderBtnClicked()
{
    const int folderId = selectedFolderId();
    if (!m_dao.softDeleteFolder || folderId == 0)
        return;

    const auto *cur = tree->currentItem();
    const QString name = cur ? cur->text(0) : QString();
    if (QMessageBox::question(
            this, tr("Delete Folder"),
            tr("Delete folder '%1'? Its %2s stay in the database but "
               "are hidden until the folder is restored.")
                .arg(name)
                .arg(m_dao.entityTitle.toLower()))
        != QMessageBox::Yes)
        return;

    const int rc = m_dao.softDeleteFolder(folderId);
    if (rc != ACTA_DB_OK) {
        QString msg;
        switch (rc) {
        case ACTA_DB_ERR_INVALID:
            // The folder still has live child folders or live entities.
            msg = tr("Folder still has live child folders or %1s. "
                      "Delete or move them first.")
                    .arg(m_dao.entityTitle.toLower());
            break;
        case ACTA_DB_ERR_NOT_FOUND:
            msg = tr("Folder not found.");
            break;
        default:
            msg = tr("Could not delete folder: %1")
                    .arg(QString::fromUtf8(acta_db_strerror(rc)));
            break;
        }
        QMessageBox::warning(this, m_dao.entityTitle, msg);
        return;
    }

    reload();
}

void FolderTreePanel::onRestoreFolderBtnClicked()
{
    const int folderId = selectedFolderId();
    if (!m_dao.restoreFolder || folderId == 0)
        return;

    const int rc = m_dao.restoreFolder(folderId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, m_dao.entityTitle,
            tr("Could not restore folder: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }

    QMessageBox::information(this, m_dao.entityTitle,
                            tr("Folder restored."));
    reload();
}

void FolderTreePanel::onDeleteKeyPressed()
{
    // Same gating as updateButtonStates(): only the enabled delete
    // handler may fire, whichever row kind is selected.
    if (deleteBtn->isEnabled())
        onEntityDeleteClicked();
    else if (deleteFolderBtn->isEnabled())
        onDeleteFolderBtnClicked();
}

void FolderTreePanel::onRenameKeyPressed()
{
    if (renameFolderBtn->isEnabled())
        onRenameFolderBtnClicked();
}

void FolderTreePanel::onReturnKeyPressed()
{
    if (showBtn->isEnabled())
        onShowBtnClicked();
}

bool FolderTreePanel::eventFilter(QObject *obj, QEvent *event)
{
    // Delete soft-deletes the selection (entity or folder), F2 renames
    // it (folders for now), Enter opens the detail dialog. Consuming the
    // key here, before QTreeWidget's own handling, also prevents its
    // built-in inline editing on F2/Return.
    if ((obj == tree || obj == tree->viewport())
            && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<const QKeyEvent *>(event);
        if (key->modifiers() == Qt::NoModifier) {
            switch (key->key()) {
            case Qt::Key_Delete:
                onDeleteKeyPressed();
                return true;
            case Qt::Key_F2:
                onRenameKeyPressed();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                onReturnKeyPressed();
                return true;
            default:
                break;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
