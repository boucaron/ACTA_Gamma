#include "executionDialog.h"
#include "ui_executionDialog.h"

#include <QDateTime>
#include <QLocale>
#include <QStandardItem>
#include <QStandardItemModel>

#include <QtGlobal>

namespace {

QString utf8(const char *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// created_at / started_at / completed_at come back from SQLite as
// "yyyy-MM-dd HH:mm:ss".
QDateTime toDateTime(const char *iso)
{
    if (!iso || !*iso)
        return {};
    const QString s = QString::fromUtf8(iso);
    static const QList<Qt::DateFormat> formats = {
        Qt::ISODateWithMs, Qt::ISODate,
    };
    for (auto f : formats) {
        const QDateTime dt = QDateTime::fromString(s, f);
        if (dt.isValid())
            return dt;
    }
    return QLocale::c().toDateTime(s, "yyyy-MM-dd HH:mm:ss");
}

} // namespace

ExecutionDialog::ExecutionDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_executionDialog)
{
    ui->setupUi(this);
    setWindowTitle("Execution");
    // connect your buttons, validators, etc. here
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
    context_t *ctx = acta_db_context_get(db, e->context_id, &err);
    if (ctx) {
        ui->contextTextEdit->setPlainText(utf8(ctx->content));
        acta_db_context_free(ctx);
    }
    skill_revision_t *srev =
        acta_db_skill_revision_get(db, e->skill_revision_id, &err);
    if (srev) {
        ui->skillLineEdit->setText(
            QStringLiteral("%1 (rev %2)")
                .arg(utf8(srev->name)).arg(srev->revision));
        acta_db_skill_revision_free(srev);
    }
    model_revision_t *mrev =
        acta_db_model_revision_get(db, e->model_revision_id, &err);
    if (mrev) {
        ui->modelLineEdit->setText(
            QStringLiteral("%1 (rev %2)")
                .arg(utf8(mrev->name)).arg(mrev->revision));
        acta_db_model_revision_free(mrev);
    }

    // Prompt
    ui->promptTextEdit->setPlainText(utf8(e->prompt));

    // Output
    ui->resultTextEdit->setPlainText(utf8(e->result));

    // Execution state
    ui->statusLineEdit->setText(utf8(e->status));
    ui->errorTextEdit->setPlainText(utf8(e->error));
    ui->rawResponseTextEdit->setPlainText(utf8(e->raw_response));

    // Metadata
    ui->creationDateTimeEdit->setDateTime(toDateTime(e->created_at));
    ui->startDateTimeEdit->setDateTime(toDateTime(e->started_at));
    ui->completedAtTimeEdit->setDateTime(toDateTime(e->completed_at));

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
            model->appendRow({
                new QStandardItem(utf8(logs[i]->created_at)),
                new QStandardItem(utf8(logs[i]->level)),
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
