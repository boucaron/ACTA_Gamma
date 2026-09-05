#include "folderTreePanel.h"

#include <QCheckBox>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSet>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHash>
#include <QMenu>

#include <functional>

#include "util.h"

namespace {
const int RoleFolderId        = Qt::UserRole;
const int RoleEntityId        = Qt::UserRole + 1;
const int RoleIsDeleted       = Qt::UserRole + 2; // entities only
const int RoleFolderIsDeleted = Qt::UserRole + 3; // folders only
const int RoleEntityFolderId  = Qt::UserRole + 4; // entity's folder (0 = root)
} // namespace

FolderTreePanel::FolderTreePanel(FolderTreeDao dao, QWidget *parent)
    : QWidget(parent), m_dao(std::move(dao))
{
    auto *lay = new QVBoxLayout(this);
    // Panel section header (P2 / UR #21): the objectName targets the
    // #panelHeader rule of the app stylesheet.
    auto *titleLabel = new QLabel(m_dao.entityTitle);
    titleLabel->setObjectName(QStringLiteral("panelHeader"));
    lay->addWidget(titleLabel);

    // Case-insensitive substring filter above the tree (H4 / UR #38);
    // folders stay visible when one of their descendants matches.
    filterEdit = new QLineEdit;
    filterEdit->setPlaceholderText(
        tr("Filter %1s or folders…").arg(m_dao.entityTitle.toLower()));
    lay->addWidget(filterEdit);

    tree = new QTreeWidget;
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    lay->addWidget(tree);

    // Centered placeholder over the blank tree when the db is empty
    // (P5 / UR #31); shown/hidden in reload(), kept centered on resize
    // in eventFilter().
    emptyLabel = makeEmptyStateLabel(
        tree, tr("No %1s yet — click New").arg(m_dao.entityTitle.toLower()));

    m_deletedIcon = tree->style()->standardIcon(QStyle::SP_TrashIcon);
    m_folderIcon  = tree->style()->standardIcon(QStyle::SP_DirIcon);

    // "Show trash" (UR #37): the deleted rows stay in the database and
    // can be restored, so "trash" is the accurate (and shorter) label.
    showDeletedCheck = new QCheckBox(tr("Show trash"));
    showDeletedCheck->setToolTip(
        tr("Show soft-deleted %1s and folders (the trash); they stay in "
           "the database and can be restored.")
            .arg(m_dao.entityTitle.toLower()));
    lay->addWidget(showDeletedCheck);

    // One icon-only toolbar row per panel (P2 / UR #22): entity
    // actions, a separator, then folder actions. The tooltips carry
    // the meaning — icons are shared across groups (trash for "Delete"
    // and "Delete Folder", SP_DialogResetButton for "Edit" and "Rename
    // Folder", SP_ArrowBack for both restores). Accelerators are
    // Alt+letter (UR #39) via setShortcut, all letters unique per
    // panel.
    const QString noun = m_dao.entityTitle.toLower();
    auto *btnStyle = style();
    auto mkBtn = [btnStyle](QStyle::StandardPixmap sp, const QString &tip,
                            const QKeySequence &sc) {
        return makeActionButton(btnStyle->standardIcon(sp), tip, sc);
    };

    auto *toolbarRow = new QHBoxLayout;
    newBtn = mkBtn(QStyle::SP_DialogYesButton,
                   tr("Create a new %1 in the selected folder "
                      "(or at the root level)").arg(noun),
                   QKeySequence(Qt::ALT | Qt::Key_N));
    showBtn = mkBtn(QStyle::SP_DialogOpenButton,
                    tr("Show the details of the selected %1 "
                       "(read-only)").arg(noun),
                    QKeySequence(Qt::ALT | Qt::Key_H));
    editBtn = mkBtn(QStyle::SP_DialogResetButton,
                    tr("Edit the selected %1 (saving creates a new "
                       "revision)").arg(noun),
                    QKeySequence(Qt::ALT | Qt::Key_E));
    deleteBtn = mkBtn(QStyle::SP_TrashIcon,
                      tr("Soft-delete the selected %1 (it stays in "
                         "the database and can be restored)").arg(noun),
                      QKeySequence(Qt::ALT | Qt::Key_D));
    restoreBtn = mkBtn(QStyle::SP_ArrowBack,
                       tr("Restore the selected soft-deleted %1").arg(noun),
                       QKeySequence(Qt::ALT | Qt::Key_T));
    toolbarRow->addWidget(newBtn);
    toolbarRow->addWidget(showBtn);
    toolbarRow->addWidget(editBtn);
    toolbarRow->addWidget(deleteBtn);
    toolbarRow->addWidget(restoreBtn);

    auto *groupSep = new QFrame;
    groupSep->setFrameShape(QFrame::VLine);
    groupSep->setFrameShadow(QFrame::Plain);
    toolbarRow->addWidget(groupSep);

    newFolderBtn = mkBtn(QStyle::SP_DirIcon,
                         tr("Create a new folder in the selected "
                            "folder (or at the root level)"),
                         QKeySequence(Qt::ALT | Qt::Key_F));
    renameFolderBtn = mkBtn(QStyle::SP_DialogResetButton,
                           tr("Rename the selected folder"),
                           QKeySequence(Qt::ALT | Qt::Key_R));
    deleteFolderBtn = mkBtn(QStyle::SP_TrashIcon,
                           tr("Soft-delete the selected folder (its "
                              "%1s are hidden until the folder is "
                              "restored)").arg(noun),
                           QKeySequence(Qt::ALT | Qt::Key_L));
    restoreFolderBtn = mkBtn(QStyle::SP_ArrowBack,
                            tr("Restore the selected soft-deleted "
                               "folder"),
                            QKeySequence(Qt::ALT | Qt::Key_O));
    toolbarRow->addWidget(newFolderBtn);
    toolbarRow->addWidget(renameFolderBtn);
    toolbarRow->addWidget(deleteFolderBtn);
    toolbarRow->addWidget(restoreFolderBtn);
    toolbarRow->addStretch();
    lay->addLayout(toolbarRow);

    connect(showDeletedCheck, &QCheckBox::toggled, this, [this](bool) {
        reload();
    });
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        applyTreeFilter(tree, t);
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
                emitItemChanged();
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

void FolderTreePanel::setDao(FolderTreeDao dao)
{
    m_dao = std::move(dao);
    reload();
}

void FolderTreePanel::reload()
{
    // Preserve the current selection (and, when nothing stays
    // selected, the scroll position) across the rebuild.
    const auto *cur = tree->currentItem();
    const int keepFolder = cur ? cur->data(0, RoleFolderId).toInt() : 0;
    const int keepEntity  = cur ? cur->data(0, RoleEntityId).toInt() : 0;
    const int keepScroll = tree->verticalScrollBar()->value();

    tree->clear();
    if (!m_dao.listFolders) {
        emptyLabel->setVisible(true);
        return; // db open failed at startup; MainWindow surfaces the reason.
    }

    // Folder skeleton: all folders in one call, nested via parent_id
    // (0 = root). list_all is ordered by id, which is not guaranteed to
    // be parent-before-child, so build the child->parent map first and
    // create each item directly under its parent (QTreeWidgetItem
    // cannot be re-parented once created).
    // Folders whose parent is not in the set stay at the top level.
    // With "Show trash" checked, soft-deleted folders are
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

    // Entities: one list_all-style query, grouped by folder in C++
    // (UR #8 / P8a) — 2 queries per reload instead of N + 1. Both
    // listers are ordered by id, so grouping a global id-ordered stream
    // keeps each folder's rows in ascending id order, exactly as the
    // old per-folder queries produced.
    const QList<FolderRow> entities = m_dao.listAllEntities(showDeleted);
    QHash<int, QList<FolderRow>> byFolder;
    for (const FolderRow &e : entities)
        byFolder[e.folderId].append(e);
    addEntities(nullptr, byFolder.value(0));
    for (auto it = folderItems.cbegin(); it != folderItems.cend(); ++it)
        addEntities(it.value(), byFolder.value(it.key()));

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        tree->topLevelItem(i)->setExpanded(true);

    // Re-select the previously current item (folder first: a deleted
    // folder can vanish from a live-only rebuild). When nothing stays
    // selected, restore the previous scroll position instead (the
    // selection's own scroll from setCurrentItem is kept otherwise).
    QTreeWidgetItem *keep = nullptr;
    if (keepFolder != 0)
        keep = findItemByRole(RoleFolderId, keepFolder);
    else if (keepEntity != 0)
        keep = findItemByRole(RoleEntityId, keepEntity);
    if (keep)
        tree->setCurrentItem(keep);
    else
        tree->verticalScrollBar()->setValue(keepScroll);

    // Re-apply the filter to the freshly built tree (H4 / UR #38).
    applyTreeFilter(tree, filterEdit ? filterEdit->text() : QString());

    // Empty-state placeholder: only when the tree has no rows at all
    // (a filter that hides every row leaves the tree blank, but that
    // is not the "no data yet" case).
    emptyLabel->setVisible(tree->topLevelItemCount() == 0);

    updateButtonStates();
    // Notify about the (possibly changed) selection after the rebuild
    // (UR #19); the last-emitted id guard suppresses duplicates when
    // the selection was preserved across the reload.
    emitItemChanged();
}

void FolderTreePanel::emitItemChanged()
{
    const int id = selectedEntityId();
    if (id == m_lastEmittedId)
        return;
    m_lastEmittedId = id;
    Q_EMIT itemChanged(id);
}

void FolderTreePanel::addEntities(QTreeWidgetItem *parent,
                                 const QList<FolderRow> &rows)
{
    for (const FolderRow &e : rows) {
        auto *item = parent
            ? new QTreeWidgetItem(parent, {e.name})
            : new QTreeWidgetItem(tree, {e.name});
        item->setData(0, RoleEntityId, e.id);
        // Remember the entity's own folder so the New button can fall
        // back to it when this entity (not a folder) is selected (UR #13).
        item->setData(0, RoleEntityFolderId, e.folderId);
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

int FolderTreePanel::newTargetFolderId() const
{
    // Target folder for the New button (UR #13): the selected folder when
    // a folder is selected, otherwise the selected entity's own folder
    // (falls back to 0 = root level when the entity is at the root).
    const auto *cur = tree->currentItem();
    if (!cur)
        return 0;
    if (cur->data(0, RoleEntityId).toInt() != 0)
        return cur->data(0, RoleEntityFolderId).toInt();
    return cur->data(0, RoleFolderId).toInt();
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

    // New entity goes into the selected folder when a folder is selected,
    // or into the selected entity's own folder when that is selected,
    // falling back to the root level (UR #13). The dialog reports whether
    // it persisted the new entity; a cancelled dialog changed nothing, so
    // no rebuild is needed (UR #8).
    if (m_dao.openNew(this, newTargetFolderId()))
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

    // Same as New: reload only when the dialog actually saved (UR #8).
    if (m_dao.openEdit(this, entityId))
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
    // A right-click on a row selects it, so the handlers operate on it
    // exactly as the toolbar row does. A right-click in the empty area
    // keeps the current selection: the menu is still shown, with the
    // row-scoped actions disabled (mirrors ContextPanel, L1).
    if (item)
        tree->setCurrentItem(const_cast<QTreeWidgetItem *>(item));
    // Empty-area menu: no item is targeted, so the creation actions
    // must go to the root level, not to a stale selection (L1).
    const bool onEmptyArea = !item;

    const bool hasEntity =
        item && item->data(0, RoleEntityId).toInt() != 0;
    const bool isEntityDeleted =
        hasEntity && item->data(0, RoleIsDeleted).toBool();
    const bool hasFolder =
        item && item->data(0, RoleFolderId).toInt() != 0;
    const bool isFolderDeleted =
        hasFolder && item->data(0, RoleFolderIsDeleted).toBool();

    QMenu menu(this);
    auto *aNew = menu.addAction(tr("New"));
    connect(aNew, &QAction::triggered, this, [this, onEmptyArea] {
        if (!m_dao.openNew)
            return;
        if (m_dao.openNew(this, onEmptyArea ? 0 : newTargetFolderId()))
            reload();
    });
    auto *aShow = menu.addAction(tr("Show"), this,
                                &FolderTreePanel::onShowBtnClicked);
    auto *aEdit = menu.addAction(tr("Edit %1").arg(m_dao.entityTitle), this,
                                &FolderTreePanel::onEditBtnClicked);
    auto *aDelete = menu.addAction(tr("Delete"), this,
                                  &FolderTreePanel::onEntityDeleteClicked);
    auto *aRestore = menu.addAction(tr("Restore"), this,
                                   &FolderTreePanel::onEntityRestoreClicked);
    menu.addSeparator();
    auto *aNewFolder = menu.addAction(tr("New Folder"));
    connect(aNewFolder, &QAction::triggered, this, [this, onEmptyArea] {
        createFolderWithParent(onEmptyArea ? 0 : selectedFolderId());
    });
    auto *aRenameFolder = menu.addAction(tr("Rename Folder"), this,
                                         &FolderTreePanel::onRenameFolderBtnClicked);
    auto *aDeleteFolder = menu.addAction(tr("Delete Folder"), this,
                                         &FolderTreePanel::onDeleteFolderBtnClicked);
    auto *aRestoreFolder = menu.addAction(tr("Restore Folder"), this,
                                          &FolderTreePanel::onRestoreFolderBtnClicked);

    // Same icons as the toolbar row (P7/P8): the context menu mirrors
    // it, so the two affordances stay aligned.
    auto *mStyle = style();
    aNew->setIcon(mStyle->standardIcon(QStyle::SP_DialogYesButton));
    aShow->setIcon(mStyle->standardIcon(QStyle::SP_DialogOpenButton));
    aEdit->setIcon(mStyle->standardIcon(QStyle::SP_DialogResetButton));
    aDelete->setIcon(mStyle->standardIcon(QStyle::SP_TrashIcon));
    aRestore->setIcon(mStyle->standardIcon(QStyle::SP_ArrowBack));
    aNewFolder->setIcon(mStyle->standardIcon(QStyle::SP_DirIcon));
    aRenameFolder->setIcon(mStyle->standardIcon(QStyle::SP_DialogResetButton));
    aDeleteFolder->setIcon(mStyle->standardIcon(QStyle::SP_TrashIcon));
    aRestoreFolder->setIcon(mStyle->standardIcon(QStyle::SP_ArrowBack));

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
    // New folder goes into the selected folder when one is selected,
    // otherwise at the root level.
    createFolderWithParent(selectedFolderId());
}

void FolderTreePanel::createFolderWithParent(int parentId)
{
    if (!m_dao.createFolder)
        return;

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
            friendlyDbError(rc, tr("folder"), trimmed,
                            tr("Could not create %1: %2")));
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
            friendlyDbError(rc, tr("folder"), trimmed,
                            tr("Could not rename %1: %2")));
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

    const auto *cur = tree->currentItem();
    const QString name = cur ? cur->text(0) : QString();

    const int rc = m_dao.restoreFolder(folderId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, m_dao.entityTitle,
            friendlyDbError(rc, tr("folder"), name,
                            tr("Could not restore %1: %2")));
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
    // F2 renames the selection (UR #39): an entity opens the Edit dialog
    // (saving creates a new revision); a folder is renamed in place.
    if (editBtn->isEnabled())
        onEditBtnClicked();
    else if (renameFolderBtn->isEnabled())
        onRenameFolderBtnClicked();
}

void FolderTreePanel::onReturnKeyPressed()
{
    if (showBtn->isEnabled())
        onShowBtnClicked();
}

bool FolderTreePanel::eventFilter(QObject *obj, QEvent *event)
{
    // Keep the empty-state label centered when the viewport resizes
    // (P5 / UR #31); the event passes through to the viewport.
    if (obj == tree->viewport() && event->type() == QEvent::Resize) {
        placeEmptyStateLabel(emptyLabel, tree);
        return QWidget::eventFilter(obj, event);
    }
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
