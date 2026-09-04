#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>
#include <QProcess>
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QTableView;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;

#include "acta_db.h"

class ExecutionPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExecutionPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QLabel *emptyLabel; // centered placeholder when the list is empty (P5 / UR #31)
    QLineEdit *filterEdit; // case-insensitive substring filter (H4 / UR #38)
    QComboBox *statusFilter; // "All" + one entry per execution status (H4 / UR #38)
    QTableView *logList; // flat log table (UR #23; setModel is public, unlike
                         // QTableWidget's)
    QLabel *emptyLogLabel; // centered placeholder when the log list is empty (P5 / UR #31)
    // Opens the execution dialog for the selected execution row; the
    // inline log list below is the only log detail view (P5 / UR #41).
    QPushButton *showDetailsBtn;
    // Opens the create dialog (new pending execution row); pairs with
    // Show the way the Context panel's New/Show pair does (UR #22).
    QPushButton *newExecutionBtn;
    // "Run" action (R1 / Plan D): spawns acta_runner run <id> for the
    // selected row; enabled only while the row is pending and no
    // runner process is active.
    QPushButton *runBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Re-point the panel at a new db handle (database switched) and
    // reload the list. Kills any active runner (Plan D): the process
    // would otherwise keep writing to the stale database.
    void setDb(db_t *db);
    // Same, carrying the database file path the "Run" button (R1)
    // passes to acta_runner via --db.
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

private slots:
    // R1 (Plan D): spawn acta_runner run <id> for the selected row, then
    // poll the DB for live status + phase log rows (UR #18 remainder,
    // UR #44).
    void onRunBtnClicked();
    void onRunnerFinished(int exitCode, QProcess::ExitStatus status);
    void onRunnerError(QProcess::ProcessError error);
    void onPollTick();

private:
    // Stop the poll timer and kill/delete the runner process (used by
    // setDb when the database switches underneath an active runner).
    void stopRunner();

    // Enable state for the Run button / menu entry: db available, a
    // pending row selected, and no active runner process.
    void updateRunBtnState();

    // Targeted refresh of the running row's status cell (no reload):
    // preserves selection, scroll and filters while the runner works.
    void refreshRunningRow();

    // Refresh the log list of the running row while it is selected,
    // auto-scrolling to the newest row (UR #44) only when the user was
    // already at the last row.
    void refreshRunningLogs();

    // Find the tree row storing executionId in column 0 (RoleExecutionId).
    QTreeWidgetItem *findRow(int executionId) const;

    // Locate the acta_runner executable: next to the app binary, then
    // PATH; empty string when not found.
    QString findRunnerExe() const;

    // R1 state (Plan D).
    QString m_dbPath;
    QProcess *m_runner = nullptr;
    QTimer *m_pollTimer = nullptr;
    int m_runningExecutionId = 0;
};
