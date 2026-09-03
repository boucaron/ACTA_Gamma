#pragma once

#include <QPoint>
#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;

#include "acta_db.h"

class ExecutionPanel : public QWidget {
public:
    explicit ExecutionPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QLabel *emptyLabel; // centered placeholder when the list is empty (P5 / UR #31)
    QLineEdit *filterEdit; // case-insensitive substring filter (H4 / UR #38)
    QComboBox *statusFilter; // "All" + one entry per execution status (H4 / UR #38)
    QTreeWidget *logList;
    QLabel *emptyLogLabel; // centered placeholder when the log list is empty (P5 / UR #31)
    // Opens the execution dialog for the selected execution row; the
    // inline log list below is the only log detail view (P5 / UR #41).
    QPushButton *showDetailsBtn;
    // Opens the create dialog (new pending execution row); pairs with
    // Show the way the Context panel's New/Show pair does (UR #22).
    QPushButton *newExecutionBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Re-point the panel at a new db handle (database switched) and
    // reload the list.
    void setDb(db_t *db);

private slots:
    // Open the read-only execution dialog for the given execution
    // (double-click on the execution list).
    void onExecutionDoubleClicked(QTreeWidgetItem *item, int column);

    // New button: open the create dialog; reload + select the new row
    // only when a row was actually created (UR #8).
    void onNewBtnClicked();

private:
    db_t *m_db;

    // Fill the log list with the log lines of the selected execution
    // (or clear it when the selection leaves an execution row).
    void showExecutionLogs(QTreeWidgetItem *item);

    // Hide execution rows that fail either the substring filter, the
    // status filter (or both). The list is flat, so a plain per-row
    // check suffices (H4 / UR #38).
    void applyFilters();

    // Keyboard accelerator (UR #39), active while the execution list has
    // focus: Enter opens the same dialog as a double-click on the row.
    // Intercepted in eventFilter() so it fires before the list's own key
    // handling (which would start inline editing on Return).
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onReturnKeyPressed();

    // Right-click context menu on the execution list: "Show" opens the
    // same dialog as double-click / Enter / Show.
    void onListContextMenu(const QPoint &pos);
};
