#include "modelDialog.h"
#include "ui_modelDialog.h"

#include <QDateTime>
#include <QLocale>

#include <QtGlobal>

namespace {

QString utf8(const char *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// created_at/updated_at come back from SQLite's now() as
// "yyyy-MM-dd HH:mm:ss"; deleted_at is NULL for live rows.
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

ModelDialog::ModelDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_modelDialog)
{
    ui->setupUi(this);
    setWindowTitle("Model");
    // connect your buttons, validators, etc. here
}

ModelDialog::~ModelDialog()
{
    delete ui;
}

void ModelDialog::editModel(db_t *db, int modelId)
{
    int err = ACTA_DB_OK;
    model_t *m = acta_db_model_get(db, modelId, &err);
    if (!m) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_get(%d) failed: %s", modelId,
                     acta_db_strerror(err));
        return;
    }

    // General
    ui->nameLineEdit->setText(utf8(m->name));
    ui->descriptionTextEdit->setPlainText(utf8(m->description));
    ui->modelTextEdit->setPlainText(utf8(m->model_identifier));

    // Configuration
    ui->baseUrlLineEdit->setText(utf8(m->base_url));
    ui->backendTextEdit->setPlainText(utf8(m->backend));
    ui->configurationTextEdit->setPlainText(utf8(m->configuration));

    // Metadata
    ui->creationDateTimeEdit->setDateTime(toDateTime(m->created_at));
    ui->updateDateTimeEdit->setDateTime(toDateTime(m->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(m->deleted_at));

    // Revision (latest snapshot for this model).
    err = ACTA_DB_OK;
    model_revision_t *rev = acta_db_model_revision_get_latest(db, modelId, &err);
    if (rev) {
        ui->revisionLineEdit->setText(QString::number(rev->revision));
        acta_db_model_revision_free(rev);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_model_revision_get_latest(%d) failed: %s", modelId,
                 acta_db_strerror(err));
    }

    acta_db_model_free(m);
}
