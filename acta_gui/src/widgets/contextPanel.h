#pragma once
#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QIcon;
class QLabel;
class QPoint;

#include "acta_db.h"

class ContextPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContextPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QLabel *emptyLabel; // centered placeholder when the list is empty (P5 / UR #31)
    QLineEdit *filterEdit; // case-insensitive substring filter (H4 / UR #38)
    QTextEdit *editor;

    QPushButton *newBtn;
    QPushButton *showBtn;
    // "Show trash": also lists soft-deleted contexts so they can be
    // restored (mirrors the skill/model/execution panels); unchecked by
    // default, so deleted rows are hidden.
    QCheckBox *showDeletedCheck;
    // Soft-delete lifecycle for the selected row (mirrors the
    // model/skill panels): Delete is enabled for live rows; Restore is
    // enabled only for deleted rows.
    QPushButton *deleteBtn;
    QPushButton *restoreBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Re-point the panel at a new db handle (database switched) and
    // reload the list.
    void setDb(db_t *db);

signals:
    // The selected context changed (id; 0 when no row is selected).
    // Emitted only on actual change, from the selection handler and
    // after a reload (UR #19). No consumers yet — future features hook
    // in here.
    void itemChanged(int id);

private:
    db_t *m_db;
    // Last id passed to itemChanged; emitItemChanged() suppresses
    // duplicate emissions (UR #19).
    int m_lastEmittedId = 0;
    // Trash icon marking soft-deleted rows when "Show trash" is on.
    QIcon m_deletedIcon;

    // Fill the textarea below the list with the content of the selected
    // context (or clear it when the selection leaves a context row).
    // It is the quick content view; the full read-only details (type,
    // hash, metadata, dates) are opened with showBtn.
    void showContext(QTreeWidgetItem *item);

private:
    // Emit itemChanged only when the selected context id actually
    // changed (UR #19).
    void emitItemChanged();

    // Enable state for the Delete / Restore buttons / menu entries:
    // Delete for a live row; Restore for a deleted row.
    void updateActionBtnStates();

    // Keep emptyLabel centered when the viewport resizes (P5 / UR #31);
    // the event passes through to the viewport.
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onNewBtnClicked();

    // Show button: the selected context in the read-only ContextDialog
    // (all fields: type, content, hash, metadata, dates). Contexts are
    // immutable, so nothing changes when the dialog closes.
    void onShowBtnClicked();

    // Soft-delete the selected live context, with a confirmation prompt;
    // the row stays in the database and can be restored.
    void onContextDeleteClicked();

    // Restore the selected deleted context (content is untouched).
    void onContextRestoreClicked();

    // Delete-key accelerator (mirrors FolderTreePanel): soft-deletes the
    // selection when Delete is enabled, otherwise restores it when
    // Restore is enabled.
    void onDeleteKeyPressed();

    // Right-click context menu on the list: "New…", "Delete", "Restore"
    // and "Show". No "Edit": contexts are immutable (acta_db has no
    // update API; the schema trigger aborts out-of-band updates).
    void onListContextMenu(const QPoint &pos);
};
