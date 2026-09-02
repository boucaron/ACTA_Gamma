#pragma once

#include <QPoint>
#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

#include "acta_db.h"

class ExecutionPanel : public QWidget {
public:
    explicit ExecutionPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QTreeWidget *logList;
    QPushButton *showBtn;
    // Opens the execution dialog for the selected execution row (the
    // "Show Log Details" button below works on the log list instead).
    QPushButton *showDetailsBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

private slots:
    // Open the read-only execution dialog for the given execution
    // (double-click on the execution list).
    void onExecutionDoubleClicked(QTreeWidgetItem *item, int column);

private:
    db_t *m_db;

    // Fill the log list with the log lines of the selected execution
    // (or clear it when the selection leaves an execution row).
    void showExecutionLogs(QTreeWidgetItem *item);

    // Open the read-only log dialog for the given log id.
    void showLogDialog(int logId);

private slots:
    void onShowBtnClicked();
    // Keyboard accelerator (UR #39), active while the execution list has
    // focus: Enter opens the same dialog as a double-click on the row.
    // Intercepted in eventFilter() so it fires before the list's own key
    // handling (which would start inline editing on Return).
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onReturnKeyPressed();

    // Right-click context menu on the execution list: "Show" opens the
    // same dialog as double-click / Enter / Show.
    void onListContextMenu(const QPoint &pos);

    // Right-click context menu on the log list: "Show" opens the same
    // dialog as the "Show Log Details" button.
    void onLogListContextMenu(const QPoint &pos);
};
