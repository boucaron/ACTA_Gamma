#include "executionLogDialog.h"
#include "ui_executionLogDialog.h"

#include "util.h"

ExecutionLogDialog::ExecutionLogDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_executionLogDialog)
{
    ui->setupUi(this);
    setWindowTitle("Log");

    // Read-only viewer: every field is filled by showLog().
    ui->levelLineEdit->setReadOnly(true);
    ui->eventLineEdit->setReadOnly(true);
    ui->messageTextEdit->setReadOnly(true);
    ui->metadataTextEdit->setReadOnly(true);
}

ExecutionLogDialog::~ExecutionLogDialog()
{
    delete ui;
}

void ExecutionLogDialog::showLog(db_t *db, int logId)
{
    m_db = db;

    int err = ACTA_DB_OK;
    execution_log_t *log = acta_db_execution_log_get(db, logId, &err);
    if (!log) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_log_get(%d) failed: %s", logId,
                     acta_db_strerror(err));
        return;
    }

    // Log tab: level, event, message.
    ui->levelLineEdit->setText(utf8(log->level));
    ui->eventLineEdit->setText(utf8(log->event));
    ui->messageTextEdit->setPlainText(utf8(log->message));

    // Metadata tab: raw metadata payload, created at.
    ui->metadataTextEdit->setPlainText(utf8(log->metadata));
    ui->createAtDateTimeEdit->setDateTime(toDateTime(log->created_at));

    acta_db_execution_log_free(log);
}
