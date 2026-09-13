#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>
class QThread;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QTableView;
class QPushButton;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QIcon;
class RunnerWorker;

#include "acta_db.h"

class ExecutionPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExecutionPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QLabel *emptyLabel; // centered placeholder when the list is empty (P5 / UR #31)
    QLineEdit *filterEdit; // case-insensitive substring filter (H4 / UR #38)
    QComboBox *statusFilter; // "All" + one entry per execution status (H4 / UR #38)
    QCheckBox *showDeletedCheck; // "Show trash": also lists soft-deleted
                                 // executions so they can be restored
    QTableView *logList; // flat log table (UR #23; setModel is public, unlike
                         // QTableWidget's)
    QLabel *emptyLogLabel; // centered placeholder when the log list is empty (P5 / UR #31)
    // Opens the execution dialog for the selected execution row; the
    // inline log list below is the only log detail view (P5 / UR #41).
    QPushButton *showDetailsBtn;
    // Opens the create dialog (new pending execution row); pairs with
    // Show the way the Context panel's New/Show pair does (UR #22).
    QPushButton *newExecutionBtn;
    // "Run" action (R1 / Plan D, in-process per M1 / UR #45): runs the
    // runner pipeline (run_execution) on a background worker thread
    // for the selected row; enabled only while the row is pending and
    // no runner worker is active. While a run is in flight the button
    // toggles into "Cancel" (cooperative cancel via the runner's cancel
    // flag; the row transitions to "cancelled").
    QPushButton *runBtn;
    // Opens the execution log dialog for the selected log line of the
    // inline log list; pairs with the log list's "Show" context menu
    // entry the way showDetailsBtn pairs with the execution list's.
    QPushButton *showLogBtn;
    // Soft-delete lifecycle for the selected row (mirrors the
    // model/skill panels): Delete is enabled for live rows that are not
    // running (deleting a running row is forbidden at the C layer);
    // Restore is enabled only for deleted rows.
    QPushButton *deleteBtn;
    QPushButton *restoreBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Re-point the panel at a new db handle (database switched) and
    // reload the list. Stops any active runner worker (Plan D): the
    // worker runs to completion on its own DB connection (bounded by
    // the backend timeout) so no row stays stuck in "running".
    void setDb(db_t *db);
    // Same, carrying the database file path the "Run" button (R1)
    // worker opens its own DB connection on.
    void setDb(db_t *db, const QString &dbPath);

signals:
    // The selected execution changed (id; 0 when no row is selected).
    // Emitted only on actual change, from the selection handler and
    // after a reload (UR #19). No consumers yet — future features hook
    // in here.
    void itemChanged(int id);

private slots:
    // Open the read-only execution dialog for the given execution
    // (double-click on the execution list).
    void onExecutionDoubleClicked(QTreeWidgetItem *item, int column);

    // New button: open the create dialog; reload + select the new row
    // only when a row was actually created (UR #8).
    void onNewBtnClicked();

private:
    db_t *m_db;
    // Last id passed to itemChanged; emitItemChanged() suppresses
    // duplicate emissions (UR #19).
    int m_lastEmittedId = 0;
    // Trash icon marking soft-deleted rows when "Show trash" is on.
    QIcon m_deletedIcon;

    // Set the log table's model and (re)establish the selectionChanged
    // connection that drives the Show button (QItemView replaces the
    // selection model on every setModel).
    void setLogModel(class QStandardItemModel *model);

    // Fill the log list with the log lines of the selected execution
    // (or clear it when the selection leaves an execution row).
    void showExecutionLogs(QTreeWidgetItem *item);

    // Hide execution rows that fail either the substring filter, the
    // status filter (or both). The list is flat, so a plain per-row
    // check suffices (H4 / UR #38).
    void applyFilters();

    // Emit itemChanged only when the selected execution id actually
    // changed (UR #19).
    void emitItemChanged();

    // Keyboard accelerator (UR #39), active while the execution list has
    // focus: Enter opens the same dialog as a double-click on the row.
    // Intercepted in eventFilter() so it fires before the list's own key
    // handling (which would start inline editing on Return).
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onReturnKeyPressed();

    // Right-click context menu on the execution list: "Show" opens the
    // same dialog as double-click / Enter / Show.
    void onListContextMenu(const QPoint &pos);

    // Right-click context menu on the inline log list: "Show" opens
    // the read-only execution log dialog for the selected log line.
    void onLogListContextMenu(const QPoint &pos);

    // Open the execution log dialog for the given log line id
    // (0 = nothing to show).
    void showLogDetails(int logId);

    // Enable state for the log Show button / menu entry: a log line is
    // currently selected in the log list.
    void updateLogBtnState();

    // Log line id of the currently selected log row (0 when none).
    int selectedLogId() const;

private slots:
    // R1 (Plan D): run the selected execution through the in-process
    // runner worker, then poll the DB for live status + phase log rows
    // (UR #18 remainder, UR #44).
    void onRunBtnClicked();
    // Worker finished (queued from the worker thread): exit code per
    // runner.h plus the failure message read from the execution's log.
    void onWorkerFinished(int exitCode, const QString &message);
    void onPollTick();

    // Soft-delete the selected live (non-running) execution, with a
    // confirmation prompt; the row stays in the database and can be
    // restored.
    void onExecuteDeleteClicked();

    // Restore the selected deleted execution (status is untouched).
    void onExecuteRestoreClicked();

    // Delete-key accelerator (mirrors FolderTreePanel): soft-deletes the
    // selection when Delete is enabled, otherwise restores it when
    // Restore is enabled.
    void onDeleteKeyPressed();

private:
    ~ExecutionPanel() override; // stops the runner worker (see stopRunner)

    // Stop the poll timer and stop/delete the runner worker (used by
    // setDb when the database switches underneath an active runner and
    // by the destructor).
    void stopRunner();

    // Enable state for the Run button / menu entry: db available, a
    // pending (or failed) live row selected, and no active runner
    // worker. Deleted rows are inert: restore first.
    void updateRunBtnState();

    // Enable state for the Delete / Restore buttons / menu entries:
    // Delete for a live, non-running row; Restore for a deleted row.
    void updateActionBtnStates();

    // Targeted refresh of the running row's status cell (no reload):
    // preserves selection, scroll and filters while the runner works.
    void refreshRunningRow();

    // Refresh the log list of the running row while it is selected,
    // auto-scrolling to the newest row (UR #44) only when the user was
    // already at the last row.
    void refreshRunningLogs();

    // Find the tree row storing executionId in column 0 (RoleExecutionId).
    QTreeWidgetItem *findRow(int executionId) const;

    // R1 state (Plan D, in-process per M1 / UR #45).
    QString m_dbPath;
    QThread *m_runnerThread = nullptr;
    RunnerWorker *m_runnerWorker = nullptr;
    QTimer *m_pollTimer = nullptr;
    int m_runningExecutionId = 0;
};
