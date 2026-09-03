#include "executionDialog.h"
#include "ui_executionDialog.h"

#include <QDateTime>

#include "contextDialog.h"
#include "modelDialog.h"
#include "skillDialog.h"

#include <QStandardItem>
#include <QStandardItemModel>

#include <QtGlobal>

#include "util.h"

ExecutionDialog::ExecutionDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_executionDialog)
{
    ui->setupUi(this);
    setWindowTitle("Execution");

    // Input tab: open the read-only dialog of the associated row.
    connect(ui->showContextPushButton, &QPushButton::clicked, this, [this]() {
        if (m_contextId == 0 || !m_db)
            return;
        ContextDialog dlg(this);
        dlg.editContext(m_db, m_contextId);
        dlg.exec();
    });
    connect(ui->showSkillPushButton, &QPushButton::clicked, this, [this]() {
        if (m_skillId == 0 || !m_db)
            return;
        SkillDialog dlg(this);
        dlg.showSkill(m_db, m_skillId);
        dlg.exec();
    });
    connect(ui->showModelPushButton, &QPushButton::clicked, this, [this]() {
        if (m_modelId == 0 || !m_db)
            return;
        ModelDialog dlg(this);
        dlg.showModel(m_db, m_modelId);
        dlg.exec();
    });
}

ExecutionDialog::~ExecutionDialog()
{
    delete ui;
}

void ExecutionDialog::editExecution(db_t *db, int executionId)
{
    m_db = db;

    int err = ACTA_DB_OK;
    execution_t *e = acta_db_execution_get(db, executionId, &err);
    if (!e) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_get(%d) failed: %s", executionId,
                     acta_db_strerror(err));
        return;
    }

    // Input: context content, skill name, model name.
    m_contextId = e->context_id;
    context_t *ctx = acta_db_context_get(db, e->context_id, &err);
    if (ctx) {
        ui->contextTextEdit->setPlainText(utf8(ctx->content));
        acta_db_context_free(ctx);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_context_get(%d) failed: %s", e->context_id,
                 acta_db_strerror(err));
    }
    skill_revision_t *srev =
        acta_db_skill_revision_get(db, e->skill_revision_id, &err);
    if (srev) {
        m_skillId = srev->skill_id;
        ui->skillLineEdit->setText(
            QStringLiteral("%1 (rev %2)")
                .arg(utf8(srev->name)).arg(srev->revision));
        acta_db_skill_revision_free(srev);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_skill_revision_get(%d) failed: %s",
                 e->skill_revision_id, acta_db_strerror(err));
    }
    model_revision_t *mrev =
        acta_db_model_revision_get(db, e->model_revision_id, &err);
    if (mrev) {
        m_modelId = mrev->model_id;
        ui->modelLineEdit->setText(
            QStringLiteral("%1 (rev %2)")
                .arg(utf8(mrev->name)).arg(mrev->revision));
        acta_db_model_revision_free(mrev);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_model_revision_get(%d) failed: %s",
                 e->model_revision_id, acta_db_strerror(err));
    }

    // Prompt
    ui->promptTextEdit->setPlainText(utf8(e->prompt));

    // Output
    ui->resultTextEdit->setPlainText(utf8(e->result));

    // Execution state (colored, UR #25)
    const QString status = utf8(e->status);
    ui->statusLineEdit->setText(status);
    applyTextColor(ui->statusLineEdit, statusColor(status));
    ui->statusLineEdit->setToolTip(statusMeaning(status)); // UR #37
    ui->errorTextEdit->setPlainText(utf8(e->error));
    ui->rawResponseTextEdit->setPlainText(utf8(e->raw_response));

    // Metadata (locale-formatted by QDateTimeEdit; the exact ISO value
    // with seconds lives in the tooltips, UR #24)
    ui->creationDateTimeEdit->setDateTime(toDateTime(e->created_at));
    ui->creationDateTimeEdit->setToolTip(utf8(e->created_at));
    ui->startDateTimeEdit->setDateTime(toDateTime(e->started_at));
    ui->startDateTimeEdit->setToolTip(utf8(e->started_at));
    ui->completedAtTimeEdit->setDateTime(toDateTime(e->completed_at));
    ui->completedAtTimeEdit->setToolTip(utf8(e->completed_at));

    // Logs: one row per log line (all levels, id order).
    int n = 0;
    err = ACTA_DB_OK;
    execution_log_t **logs =
        acta_db_execution_log_list_by_execution(db, e->id, nullptr, 0, 0,
                                                &n, &err);
    auto *model = new QStandardItemModel(0, 4, ui->logsTableView);
    model->setHorizontalHeaderLabels(
        {QStringLiteral("Date"), QStringLiteral("Level"),
         QStringLiteral("Event"), QStringLiteral("Message")});
    if (logs) {
        for (int i = 0; i < n; ++i) {
            auto *dateItem = new QStandardItem(
                displayDateTime(logs[i]->created_at)); // UR #24
            dateItem->setToolTip(utf8(logs[i]->created_at));
            auto *levelItem = new QStandardItem(utf8(logs[i]->level));
            levelItem->setForeground(logLevelColor(utf8(logs[i]->level)));
            model->appendRow({
                dateItem,
                levelItem,
                new QStandardItem(utf8(logs[i]->event)),
                new QStandardItem(utf8(logs[i]->message)),
            });
        }
        acta_db_execution_log_list_free(logs, n);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_execution_log_list_by_execution(%d) failed: %s",
                 e->id, acta_db_strerror(err));
    }
    ui->logsTableView->setModel(model);

    acta_db_execution_free(e);
}
