#include "contextPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QKeySequence>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextEdit>
#include <QPushButton>
#include <QStyle>

#include "contextDialog.h"
#include "util.h"

namespace {
const int RoleContextId = Qt::UserRole;
// Marker for soft-deleted rows (visible only with "Show trash" on);
// drives the Delete / Restore button states.
const int RoleContextDeleted = Qt::UserRole + 1;
//
// The list reload uses the light projection
// (acta_db_context_query_light / _with_deleted_light): the content
// blob is NOT materialized per row, so the filter matches the visible
// columns (Type + Date) only. The full content of the selected row is
// still fetched on demand via acta_db_context_get (inline editor and
// details dialog).
} // namespace

ContextPanel::ContextPanel(db_t *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    // Panel section header (P2 / UR #21): the objectName targets the
    // #panelHeader rule of the app stylesheet.
    auto *titleLabel = new QLabel(tr("Context"));
    titleLabel->setObjectName(QStringLiteral("panelHeader"));
    lay->addWidget(titleLabel);

    // Case-insensitive substring filter above the list (H4 / UR #38).
    filterEdit = new QLineEdit;
    filterEdit->setPlaceholderText(tr("Filter contexts by type…"));
    lay->addWidget(filterEdit);

    // "Show trash" (mirrors the skill/model panels): soft-deleted
    // contexts stay in the database and can be restored, so the
    // trash label is the accurate (and shorter) one. Unchecked by
    // default: deleted rows are hidden.
    showDeletedCheck = new QCheckBox(tr("Show trash"));
    showDeletedCheck->setToolTip(
        tr("Show soft-deleted contexts (the trash); they stay in the "
           "database and can be restored."));
    lay->addWidget(showDeletedCheck);

    list = new QTreeWidget;
    list->setColumnCount(2);
    list->setHeaderLabels({tr("Type"), tr("Date")});
    list->setSortingEnabled(true);
    connect(list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showContext(cur);
                showBtn->setEnabled(
                    cur && cur->data(0, RoleContextId).toInt() != 0);
                emitItemChanged();
                updateActionBtnStates();
            });
    // Double-click opens the read-only Show dialog, the same as the
    // Show button / Enter / context menu. The clicked row is current
    // by the time the signal fires, so the shared handler works
    // unchanged (parity with the execution panel's double-click).
    connect(list, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *, int) { onShowBtnClicked(); });
    lay->addWidget(list);

    // Centered placeholder over the blank list when the db is empty
    // (P5 / UR #31); shown/hidden in reload().
    emptyLabel = makeEmptyStateLabel(list, tr("No contexts yet — click New"));

    editor = new QTextEdit;
    editor->setReadOnly(true); // contexts are immutable; this is display-only
    editor->setPlaceholderText(tr("Immutable input JSON..."));
    lay->addWidget(editor);

    // Icon-only toolbar row (P2 / UR #22), matching the other panels:
    // the tooltip carries the meaning, the accelerator is Alt+letter
    // (UR #39). The inline editor below is the quick content view; the
    // Show button opens the full read-only details dialog (type, hash,
    // metadata, dates), enabled only while a context row is selected.
    m_deletedIcon = list->style()->standardIcon(QStyle::SP_TrashIcon);

    auto *btnStyle = style();
    auto *btnRow = new QHBoxLayout;
    newBtn = makeActionButton(
        btnStyle->standardIcon(QStyle::SP_DialogYesButton),
        tr("Create a new context"),
        QKeySequence(Qt::ALT | Qt::Key_N));
    btnRow->addWidget(newBtn);
    showBtn = makeActionButton(
        btnStyle->standardIcon(QStyle::SP_DialogOpenButton),
        tr("Show the selected context (all data, read-only)"),
        QKeySequence(Qt::ALT | Qt::Key_H));
    showBtn->setEnabled(false);
    btnRow->addWidget(showBtn);
    // Soft-delete lifecycle for the selected row (mirrors the
    // model/skill panels): Delete for a live row; Restore only for
    // deleted rows.
    deleteBtn = makeActionButton(
        btnStyle->standardIcon(QStyle::SP_TrashIcon),
        tr("Soft-delete the selected context (it stays in the "
           "database and can be restored)"),
        QKeySequence(Qt::ALT | Qt::Key_D));
    btnRow->addWidget(deleteBtn);
    restoreBtn = makeActionButton(
        btnStyle->standardIcon(QStyle::SP_ArrowBack),
        tr("Restore the selected soft-deleted context"),
        QKeySequence(Qt::ALT | Qt::Key_T));
    btnRow->addWidget(restoreBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    // callback
    connect(newBtn, &QPushButton::clicked, this, &ContextPanel::onNewBtnClicked);
    connect(showBtn, &QPushButton::clicked, this,
            &ContextPanel::onShowBtnClicked);
    connect(deleteBtn, &QPushButton::clicked, this,
            &ContextPanel::onContextDeleteClicked);
    connect(restoreBtn, &QPushButton::clicked, this,
            &ContextPanel::onContextRestoreClicked);
    connect(showDeletedCheck, &QCheckBox::toggled, this, [this](bool) {
        reload();
    });
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        // Light projection: no per-row content payload; the filter
        // matches the visible columns (Type + Date).
        applyTreeFilter(list, t);
    });

    // Right-click context menu on the list: "New…" and "Show" (no
    // "Edit": contexts are immutable — see onListContextMenu()).
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QTreeWidget::customContextMenuRequested, this,
            &ContextPanel::onListContextMenu);

    // Keeps emptyLabel centered as the viewport resizes (P5 / UR #31),
    // and (UR #39) handles Enter on the list: opens the read-only Show
    // dialog. Installed on both the tree and its viewport because either
    // widget can be the focus target after a click.
    list->installEventFilter(this);
    list->viewport()->installEventFilter(this);

    reload();
}

bool ContextPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == list->viewport() && event->type() == QEvent::Resize) {
        placeEmptyStateLabel(emptyLabel, list);
        return QWidget::eventFilter(obj, event);
    }
    // Enter opens the same dialog as the Show button (UR #39), enabled
    // only while a context row is selected — the same gating as the
    // button. Consuming the key here, before QTreeWidget's own handling,
    // also prevents its built-in inline editing on Return.
    if ((obj == list || obj == list->viewport())
            && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<const QKeyEvent *>(event);
        if (key->modifiers() == Qt::NoModifier) {
            if (key->key() == Qt::Key_Return
                    || key->key() == Qt::Key_Enter) {
                if (showBtn->isEnabled())
                    onShowBtnClicked();
                return true;
            }
            // Delete soft-deletes the selection (or restores a deleted
            // row), mirroring the skill/model panel's Delete key.
            if (key->key() == Qt::Key_Delete) {
                onDeleteKeyPressed();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ContextPanel::setDb(db_t *db)
{
    m_db = db;
    reload();
}

void ContextPanel::emitItemChanged()
{
    const auto *cur = list->currentItem();
    const int id = cur ? cur->data(0, RoleContextId).toInt() : 0;
    if (id == m_lastEmittedId)
        return;
    m_lastEmittedId = id;
    Q_EMIT itemChanged(id);
}

void ContextPanel::reload()
{
    // Preserve the current selection across the rebuild (UR #8).
    const auto *cur = list->currentItem();
    const int keepContext = cur ? cur->data(0, RoleContextId).toInt() : 0;

    list->clear();
    if (!m_db) {
        emptyLabel->setVisible(true);
        return; // db open failed at startup; MainWindow surfaces the reason.
    }

    int n = 0;
    int err = ACTA_DB_OK;
    // Live rows by default (the DB layer's live-only default); with
    // "Show trash" the query also returns soft-deleted rows so they
    // can be restored.
    //
    // Light projection: the content blob is not materialized per row
    // (content stays NULL); only type / hash / dates are needed for
    // the list. The selected row's full content is fetched on demand
    // in showContext() via acta_db_context_get.
    context_t **contexts =
        (showDeletedCheck != nullptr && showDeletedCheck->isChecked())
            ? acta_db_context_query_with_deleted_light(m_db, nullptr, 0, 0,
                                                       &n, &err)
            : acta_db_context_query_light(m_db, nullptr, 0, 0, &n, &err);
    if (!contexts) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_query_light failed: %s",
                     acta_db_strerror(err));
        emptyLabel->setVisible(true);
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = new QTreeWidgetItem(list);
        item->setText(0, contexts[i]->type && contexts[i]->type[0]
                             ? QString::fromUtf8(contexts[i]->type)
                             : QStringLiteral("context"));
        // Locale-formatted date (UR #24); the display stays
        // "yyyy-MM-dd HH:mm" in every locale, so the column still
        // sorts chronologically as plain text. The exact ISO value
        // (with seconds) lives in the tooltip.
        const QString createdIso = contexts[i]->created_at
            ? QString::fromUtf8(contexts[i]->created_at)
            : QString();
        item->setText(1, displayDateTime(contexts[i]->created_at));
        item->setToolTip(1, createdIso);
        item->setData(0, RoleContextId, contexts[i]->id);

        // Soft-deleted row (only visible with "Show trash"): trash
        // icon, gray date and a deleted tooltip, mirroring the
        // skill/model/execution panel rows. The marker drives the
        // Delete / Restore button states.
        if (contexts[i]->deleted_at != nullptr) {
            item->setData(0, RoleContextDeleted, true);
            item->setIcon(0, m_deletedIcon);
            item->setForeground(1, QColor(Qt::gray));
            item->setToolTip(1, tr("Deleted %1").arg(createdIso));
        }
    }

    acta_db_context_list_free(contexts, n);

    // Re-select the previously current row (flat list: linear scan by
    // id); this also refills the inline content editor via
    // currentItemChanged.
    if (keepContext != 0)
        for (int i = 0; i < list->topLevelItemCount(); ++i) {
            auto *item = list->topLevelItem(i);
            if (item->data(0, RoleContextId).toInt() == keepContext) {
                list->setCurrentItem(item);
                break;
            }
        }

    // Stable newest-first order after the rebuild (UR #23): the DB
    // returns rows by id (insertion order); the Date column is
    // "yyyy-MM-dd HH:mm" in every locale, so a plain-text sort is
    // chronological. Rows created in the same minute sort in
    // unspecified order (the seconds live only in the tooltip).
    list->sortItems(1, Qt::DescendingOrder);

    // Re-apply the filter to the freshly built list (H4 / UR #38);
    // light projection → the filter matches the visible columns only.
    applyTreeFilter(list, filterEdit ? filterEdit->text() : QString());

    // Empty-state placeholder: only when there are no rows at all
    // (P5 / UR #31).
    emptyLabel->setVisible(list->topLevelItemCount() == 0);

    // Notify about the (possibly changed) selection after the rebuild
    // (UR #19); the last-emitted id guard suppresses duplicates when
    // the selection was preserved across the reload.
    emitItemChanged();

    // Re-evaluate the Delete / Restore buttons against the rebuilt list
    // (also covers the selection vanishing when "Show trash" is turned
    // off under a deleted row).
    updateActionBtnStates();
}

void ContextPanel::showContext(QTreeWidgetItem *item)
{
    const int contextId = item ? item->data(0, RoleContextId).toInt() : 0;
    if (contextId == 0 || !m_db) {
        editor->clear();
        return;
    }

    int err = ACTA_DB_OK;
    context_t *c = acta_db_context_get(m_db, contextId, &err);
    if (!c) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_get(%d) failed: %s", contextId,
                     acta_db_strerror(err));
        editor->clear();
        return;
    }
    editor->setPlainText(c->content ? c->content : "");
    acta_db_context_free(c);
}

void ContextPanel::onNewBtnClicked()
{
    if (!m_db)
        return;

    ContextDialog dlg(this);
    dlg.newContext(m_db);
    dlg.exec();
    // Reload only when the dialog actually created a context; a
    // cancelled dialog changed nothing (UR #8).
    if (dlg.saved())
        reload();
}

void ContextPanel::updateActionBtnStates()
{
    const auto *cur = list->currentItem();
    const bool hasRow =
        cur != nullptr && cur->data(0, RoleContextId).toInt() != 0;
    const bool isDeleted =
        hasRow && cur->data(0, RoleContextDeleted).toBool();
    deleteBtn->setEnabled(hasRow && !isDeleted);
    restoreBtn->setEnabled(hasRow && isDeleted);
}

void ContextPanel::onShowBtnClicked()
{
    const auto *cur = list->currentItem();
    const int contextId = cur ? cur->data(0, RoleContextId).toInt() : 0;
    if (contextId == 0 || !m_db)
        return;

    // Read-only view of the selected context (Mode ReadOnly, Close
    // only): every field of the row. Contexts are immutable, so
    // nothing changes when the dialog closes.
    ContextDialog dlg(this);
    dlg.editContext(m_db, contextId);
    dlg.exec();
}

void ContextPanel::onContextDeleteClicked()
{
    if (!m_db || !deleteBtn->isEnabled())
        return;
    const auto *cur = list->currentItem();
    const int contextId = cur ? cur->data(0, RoleContextId).toInt() : 0;
    if (contextId == 0)
        return;

    // Same confirmation style as the skill/model panel delete.
    if (QMessageBox::question(
            this, tr("Delete Context"),
            tr("Delete context %1? It stays in the database and can "
               "be restored.")
                .arg(contextId))
        != QMessageBox::Yes)
        return;

    const int rc = acta_db_context_delete(m_db, contextId);
    if (rc != ACTA_DB_OK) {
        // A missing row and an already-deleted row are
        // indistinguishable by design (context.h).
        QString msg;
        if (rc == ACTA_DB_ERR_NOT_FOUND)
            msg = tr("Context not found (already deleted?).");
        else
            msg = tr("Could not delete the context: %1")
                    .arg(QString::fromUtf8(acta_db_strerror(rc)));
        QMessageBox::warning(this, tr("Delete Context"), msg);
        return;
    }

    reload();
}

void ContextPanel::onContextRestoreClicked()
{
    if (!m_db || !restoreBtn->isEnabled())
        return;
    const auto *cur = list->currentItem();
    const int contextId = cur ? cur->data(0, RoleContextId).toInt() : 0;
    if (contextId == 0)
        return;

    const int rc = acta_db_context_restore(m_db, contextId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, tr("Restore Context"),
            tr("Could not restore the context: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }
    // Content is untouched by the restore (contexts are immutable).
    QMessageBox::information(this, tr("Context"),
                             tr("Context %1 restored.").arg(contextId));
    reload();
}

void ContextPanel::onDeleteKeyPressed()
{
    // Same gating as updateActionBtnStates(): Delete soft-deletes a
    // live row; Restore undeletes a deleted row.
    if (deleteBtn->isEnabled())
        onContextDeleteClicked();
    else if (restoreBtn->isEnabled())
        onContextRestoreClicked();
}

void ContextPanel::onListContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto *aNew = menu.addAction(tr("New…"));
    aNew->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    aNew->setToolTip(tr("Create a new context"));
    connect(aNew, &QAction::triggered, this, [this] { onNewBtnClicked(); });
    // Soft-delete lifecycle entries, enabled exactly like the toolbar
    // buttons (Delete: live row; Restore: deleted row).
    auto *aDelete = menu.addAction(tr("Delete"));
    aDelete->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    aDelete->setToolTip(tr("Soft-delete the selected context (it stays "
                            "in the database and can be restored)"));
    aDelete->setEnabled(deleteBtn->isEnabled());
    connect(aDelete, &QAction::triggered, this, [this] {
        onContextDeleteClicked();
    });
    auto *aRestore = menu.addAction(tr("Restore"));
    aRestore->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    aRestore->setToolTip(tr("Restore the selected soft-deleted context"));
    aRestore->setEnabled(restoreBtn->isEnabled());
    connect(aRestore, &QAction::triggered, this, [this] {
        onContextRestoreClicked();
    });
    // "Show" operates on the currently selected row, like the Show
    // button of the panel.
    auto *aShow = menu.addAction(tr("Show"));
    aShow->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    aShow->setToolTip(tr("Show the selected context (all data, read-only)"));
    aShow->setEnabled(showBtn->isEnabled());
    connect(aShow, &QAction::triggered, this,
            [this] { onShowBtnClicked(); });
    // Note: no "Edit" entry — context rows are immutable (acta_db has
    // no update API and the schema trigger aborts out-of-band
    // updates), so there is nothing to edit.
    menu.exec(list->viewport()->mapToGlobal(pos));
}
