#include "executionPanel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "executionDialog.h"

namespace {
const int RoleExecutionId = Qt::UserRole;
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
    lay->addWidget(list);

    runBtn = new QPushButton("Run One-Shot");
    lay->addWidget(runBtn);

    log = new QTextEdit;
    log->setReadOnly(true);
    log->setPlaceholderText("Execution log...");
    lay->addWidget(log);

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
        log->clear();
        return;
    }

    int n = 0;
    int err = ACTA_DB_OK;
    execution_log_t **lines =
        acta_db_execution_log_list_by_execution(m_db, executionId, nullptr,
                                                0, 0, &n, &err);
    if (!lines) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_log_list_by_execution(%d) failed: %s",
                     executionId, acta_db_strerror(err));
        log->clear();
        return;
    }

    QStringList text;
    for (int i = 0; i < n; ++i) {
        // "created_at  level: event – message" (metadata not shown).
        text << QStringLiteral("%1  %2: %3%4")
                       .arg(lines[i]->created_at
                                 ? QString::fromUtf8(lines[i]->created_at)
                                 : QString())
                       .arg(lines[i]->level
                                 ? QString::fromUtf8(lines[i]->level)
                                 : QString())
                       .arg(lines[i]->event
                                 ? QString::fromUtf8(lines[i]->event)
                                 : QString())
                       .arg(lines[i]->message && lines[i]->message[0]
                                  ? QStringLiteral(" – ")
                                    + QString::fromUtf8(lines[i]->message)
                                  : QString());
    }
    log->setPlainText(text.join(QStringLiteral("\n")));
    acta_db_execution_log_list_free(lines, n);
}

void ExecutionPanel::onShowBtnClicked()
{
    ExecutionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // TODO
    }
}
