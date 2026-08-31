#include "modelDialog.h"
#include "ui_modelDialog.h"

#include <QDateTime>
#include <QLocale>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QtGlobal>

namespace {

const int RoleRevisionId = Qt::UserRole;

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

    ui->revisionTreeWidget->setColumnCount(1);
    ui->revisionTreeWidget->setHeaderLabel("Revision");
    ui->revisionTreeWidget->setRootIsDecorated(false);
    ui->revisionTreeWidget->setUniformRowHeights(true);
    connect(ui->revisionTreeWidget, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showRevision(cur);
            });

    // connect your buttons, validators, etc. here
}

ModelDialog::~ModelDialog()
{
    delete ui;
}

void ModelDialog::editModel(db_t *db, int modelId)
{
    m_db = db;

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

    // Revisions: one row per snapshot, ascending (oldest first).
    err = ACTA_DB_OK;
    int nRevs = 0;
    model_revision_t **revs =
        acta_db_model_revision_list_by_model(db, modelId, 0, 0, &nRevs, &err);
    ui->revisionTreeWidget->clear();
    if (revs) {
        for (int i = 0; i < nRevs; ++i) {
            auto *item = new QTreeWidgetItem(ui->revisionTreeWidget);
            item->setText(0, QStringLiteral("Revision %1")
                                   .arg(revs[i]->revision));
            item->setData(0, RoleRevisionId, revs[i]->id);
        }
        acta_db_model_revision_list_free(revs, nRevs);
        // Select the latest (last, list is ascending) to show it.
        if (nRevs > 0)
            ui->revisionTreeWidget->setCurrentItem(
                ui->revisionTreeWidget->topLevelItem(nRevs - 1));
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_model_revision_list_by_model(%d) failed: %s", modelId,
                 acta_db_strerror(err));
    }

    acta_db_model_free(m);
}

void ModelDialog::showRevision(QTreeWidgetItem *item)
{
    const int revisionId =
        item ? item->data(0, RoleRevisionId).toInt() : 0;
    if (revisionId == 0 || !m_db)
        return;

    int err = ACTA_DB_OK;
    model_revision_t *rev = acta_db_model_revision_get(m_db, revisionId, &err);
    if (!rev) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_revision_get(%d) failed: %s", revisionId,
                     acta_db_strerror(err));
        return;
    }

    // General
    ui->nameLineEdit->setText(utf8(rev->name));
    ui->descriptionTextEdit->setPlainText(utf8(rev->description));
    ui->modelTextEdit->setPlainText(utf8(rev->model_identifier));

    // Configuration
    ui->baseUrlLineEdit->setText(utf8(rev->base_url));
    ui->backendTextEdit->setPlainText(utf8(rev->backend));
    ui->configurationTextEdit->setPlainText(utf8(rev->configuration));

    // Metadata
    ui->creationDateTimeEdit->setDateTime(toDateTime(rev->created_at));
    ui->updateDateTimeEdit->setDateTime(toDateTime(rev->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(rev->deleted_at));

    ui->revisionLineEdit->setText(QString::number(rev->revision));

    acta_db_model_revision_free(rev);
}
