#include "executionPanel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "executionDialog.h"
#include "executionLogDialog.h"

namespace {
const int RoleExecutionId = Qt::UserRole;
const int RoleLogId = Qt::UserRole + 1;
} // namespace

ExecutionPanel::ExecutionPanel(db_t *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Execution"));

    list = new QTreeWidget;
    list->setColumnCount(2);
    list->setHeaderLabels({"Date", "Status"});
    list->setSortingEnabled(true);
    connect(list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showExecutionLogs(cur);
            });
    connect(list, &QTreeWidget::itemDoubleClicked, this,
            &ExecutionPanel::onExecutionDoubleClicked);
    lay->addWidget(list);

    runBtn = new QPushButton("Run One-Shot");
    lay->addWidget(runBtn);

    logList = new QTreeWidget;
    logList->setColumnCount(4);
    logList->setHeaderLabels({"Date", "Level", "Event", "Message"});
    logList->setRootIsDecorated(false);
    logList->setUniformRowHeights(true);
    lay->addWidget(logList);

    showBtn = new QPushButton("Show");
    lay->addWidget(showBtn);

    // callback
    connect(showBtn, &QPushButton::clicked, this, &ExecutionPanel::onShowBtnClicked);

    reload();
}

void ExecutionPanel::reload()
{
    list->clear();
    if (!m_db)
        return; // db open failed at startup; MainWindow surfaces the reason.

    int n = 0;
    int err = ACTA_DB_OK;
    execution_t **executions =
        acta_db_execution_query(m_db, nullptr, 0, 0, &n, &err);
    if (!executions) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_query failed: %s",
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = new QTreeWidgetItem(list);
        item->setText(0, executions[i]->created_at
                             ? QString::fromUtf8(executions[i]->created_at)
                             : QString());
        item->setText(1, executions[i]->status
                             ? QString::fromUtf8(executions[i]->status)
                             : QString());
        // ISO "yyyy-MM-dd HH:mm:ss" sorts correctly as plain text.
        item->setData(0, RoleExecutionId, executions[i]->id);
    }

    acta_db_execution_list_free(executions, n);
}

void ExecutionPanel::showExecutionLogs(QTreeWidgetItem *item)
{
    const int executionId = item ? item->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0 || !m_db) {
        logList->clear();
        return;
    }

    int n = 0;
    int err = ACTA_DB_OK;
    execution_log_t **lines =
        acta_db_execution_log_list_by_execution(m_db, executionId, nullptr,
                                                0, 0, &n, &err);
    logList->clear();
    if (!lines) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_log_list_by_execution(%d) failed: %s",
                     executionId, acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *logItem = new QTreeWidgetItem(logList, {
            lines[i]->created_at
                ? QString::fromUtf8(lines[i]->created_at)
                : QString(),
            lines[i]->level ? QString::fromUtf8(lines[i]->level) : QString(),
            lines[i]->event ? QString::fromUtf8(lines[i]->event) : QString(),
            lines[i]->message
                ? QString::fromUtf8(lines[i]->message)
                : QString(),
        });
        logItem->setData(0, RoleLogId, lines[i]->id);
    }
    acta_db_execution_log_list_free(lines, n);
}

void ExecutionPanel::onExecutionDoubleClicked(QTreeWidgetItem *item, int)
{
    const int executionId =
        item ? item->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0 || !m_db)
        return;

    ExecutionDialog dlg(this);
    dlg.editExecution(m_db, executionId);
    if (dlg.exec() == QDialog::Accepted) {
        // TODO
    }
}

void ExecutionPanel::onShowBtnClicked()
{
    const QTreeWidgetItem *cur = logList->currentItem();
    const int logId = cur ? cur->data(0, RoleLogId).toInt() : 0;
    if (logId == 0 || !m_db)
        return;

    ExecutionLogDialog dlg(this);
    dlg.showLog(m_db, logId);
    dlg.exec();
}
