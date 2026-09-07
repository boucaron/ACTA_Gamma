#include "executionLogDialog.h"
#include "ui_executionLogDialog.h"

#include "util.h"

ExecutionLogDialog::ExecutionLogDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_executionLogDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Log"));
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

    // Log: level (colored, UR #25), event, message.
    const QString level = utf8(log->level);
    ui->levelLineEdit->setText(level);
    applyTextColor(ui->levelLineEdit, logLevelColor(level));
    ui->eventLineEdit->setText(utf8(log->event));
    ui->messageTextEdit->setPlainText(utf8(log->message));

    // Metadata: raw metadata + locale-formatted created_at (UR #24); the
    // exact ISO value with seconds lives in the tooltip.
    ui->metadataTextEdit->setPlainText(utf8(log->metadata));
    ui->createAtDateTimeEdit->setDateTime(toDateTime(log->created_at));
    ui->createAtDateTimeEdit->setToolTip(utf8(log->created_at));

    acta_db_execution_log_free(log);
}
