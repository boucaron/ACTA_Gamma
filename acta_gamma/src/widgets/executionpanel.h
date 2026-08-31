#pragma once

#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

#include "acta_db.h"

class ExecutionPanel : public QWidget {
public:
    explicit ExecutionPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QPushButton *runBtn;
    QTreeWidget *logList;
    QPushButton *showBtn;

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

private slots:
    void onShowBtnClicked();
};
