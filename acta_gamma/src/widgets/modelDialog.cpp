#include "modelDialog.h"
#include "ui_modelDialog.h"

#include <QDateTime>
#include <QMessageBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QtGlobal>

#include "util.h"

ModelDialog::ModelDialog(QWidget *parent)
    : EntityDialog(QStringLiteral("Model"), QStringLiteral("model"), parent),
      ui(new Ui_modelDialog)
{
    ui->setupUi(this);
    configureRevisionTree();

    setMode(Mode::ReadOnly);
}

ModelDialog::~ModelDialog()
{
    delete ui;
}

void ModelDialog::applyReadOnly(bool readOnly)
{
    ui->nameLineEdit->setReadOnly(readOnly);
    ui->descriptionTextEdit->setReadOnly(readOnly);
    ui->modelTextEdit->setReadOnly(readOnly);
    ui->baseUrlLineEdit->setReadOnly(readOnly);
    ui->backendTextEdit->setReadOnly(readOnly);
    ui->configurationTextEdit->setReadOnly(readOnly);
}

QString ModelDialog::entityName() const
{
    return ui->nameLineEdit->text();
}

void ModelDialog::newModel(db_t *db, int folderId)
{
    setDb(db);
    setEntityId(0);
    setFolderId(folderId);

    ui->nameLineEdit->clear();
    ui->descriptionTextEdit->clear();
    ui->modelTextEdit->clear();
    ui->baseUrlLineEdit->clear();
    ui->backendTextEdit->clear();
    ui->configurationTextEdit->clear();
    ui->revisionLineEdit->clear();
    ui->creationDateTimeEdit->setDateTime(QDateTime());
    ui->updateDateTimeEdit->setDateTime(QDateTime());
    ui->deleteDateTimeEdit->setDateTime(QDateTime());
    ui->revisionTreeWidget->clear();

    setMode(Mode::New);
    setWindowTitle(newTitle());
}

void ModelDialog::showModel(db_t *db, int modelId)
{
    setDb(db);
    loadEntity(modelId);
    setMode(Mode::ReadOnly);
    setWindowTitle(showTitle());
}

void ModelDialog::editModel(db_t *db, int modelId)
{
    setDb(db);
    loadEntity(modelId);
    setMode(Mode::Edit);
    setWindowTitle(editTitle());
}

void ModelDialog::loadEntity(int modelId)
{
    int err = ACTA_DB_OK;
    model_t *m = acta_db_model_get(m_db, modelId, &err);
    if (!m) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_get(%d) failed: %s", modelId,
                     acta_db_strerror(err));
        return;
    }

    m_id = m->id;
    m_folderId = m->folder_id;

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
    loadRevisionTree([this](int id, int &e) {
        int n = 0;
        model_revision_t **revs =
            acta_db_model_revision_list_by_model(m_db, id, 0, 0, &n, &e);
        QList<RevisionRow> out;
        if (revs) {
            for (int i = 0; i < n; ++i)
                out.append(RevisionRow{revs[i]->id, revs[i]->revision});
            acta_db_model_revision_list_free(revs, n);
        }
        return out;
    });

    acta_db_model_free(m);
}

bool ModelDialog::validateFields()
{
    const QString name = ui->nameLineEdit->text().trimmed();
    const QString backend = ui->backendTextEdit->toPlainText().trimmed();
    const QString modelIdentifier =
        ui->modelTextEdit->toPlainText().trimmed();
    if (name.isEmpty() || backend.isEmpty() || modelIdentifier.isEmpty()) {
        QMessageBox::warning(
            this, title(),
            tr("Name, backend and model identifier are required."));
        return false;
    }
    return true;
}

int ModelDialog::createEntity(int *outId)
{
    const QByteArray nameBa = ui->nameLineEdit->text().trimmed().toUtf8();
    const QByteArray descriptionBa =
        ui->descriptionTextEdit->toPlainText().toUtf8();
    const QByteArray baseUrlBa = ui->baseUrlLineEdit->text().toUtf8();
    const QByteArray backendBa =
        ui->backendTextEdit->toPlainText().trimmed().toUtf8();
    const QByteArray modelIdentifierBa =
        ui->modelTextEdit->toPlainText().trimmed().toUtf8();
    const QByteArray configurationBa =
        ui->configurationTextEdit->toPlainText().toUtf8();

    model_t m{};
    m.folder_id = m_folderId;
    m.name = dupString(nameBa.constData());
    m.description =
        dupString(descriptionBa.isEmpty() ? nullptr
                                           : descriptionBa.constData());
    m.backend = dupString(backendBa.constData());
    m.base_url =
        dupString(baseUrlBa.isEmpty() ? nullptr : baseUrlBa.constData());
    m.model_identifier = dupString(modelIdentifierBa.constData());
    m.configuration =
        dupString(configurationBa.isEmpty() ? nullptr
                                              : configurationBa.constData());

    const int rc = acta_db_model_create(m_db, &m, outId);
    std::free(m.name);
    std::free(m.description);
    std::free(m.backend);
    std::free(m.base_url);
    std::free(m.model_identifier);
    std::free(m.configuration);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, title(),
            tr("Could not create %1: %2").arg(noun())
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
    }
    return rc;
}

int ModelDialog::updateEntity()
{
    const QByteArray nameBa = ui->nameLineEdit->text().trimmed().toUtf8();
    const QByteArray descriptionBa =
        ui->descriptionTextEdit->toPlainText().toUtf8();
    const QByteArray baseUrlBa = ui->baseUrlLineEdit->text().toUtf8();
    const QByteArray backendBa =
        ui->backendTextEdit->toPlainText().trimmed().toUtf8();
    const QByteArray modelIdentifierBa =
        ui->modelTextEdit->toPlainText().trimmed().toUtf8();
    const QByteArray configurationBa =
        ui->configurationTextEdit->toPlainText().toUtf8();

    // Full-row update: fetch the live row, replace the editable
    // fields, write it back.
    int err = ACTA_DB_OK;
    model_t *m = acta_db_model_get_live(m_db, m_id, &err);
    if (!m) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_get_live(%d) failed: %s", m_id,
                     acta_db_strerror(err));
        return err != ACTA_DB_OK ? err : ACTA_DB_ERR_INVALID;
    }
    std::free(m->name);
    std::free(m->description);
    std::free(m->backend);
    std::free(m->base_url);
    std::free(m->model_identifier);
    std::free(m->configuration);
    m->name = dupString(nameBa.constData());
    m->description =
        dupString(descriptionBa.isEmpty() ? nullptr
                                           : descriptionBa.constData());
    m->backend = dupString(backendBa.constData());
    m->base_url =
        dupString(baseUrlBa.isEmpty() ? nullptr : baseUrlBa.constData());
    m->model_identifier = dupString(modelIdentifierBa.constData());
    m->configuration =
        dupString(configurationBa.isEmpty() ? nullptr
                                              : configurationBa.constData());

    const int rc = acta_db_model_update(m_db, m);
    // We own these duplicates now; null them so acta_db_model_free
    // only releases what it allocated.
    std::free(m->name);
    std::free(m->description);
    std::free(m->backend);
    std::free(m->base_url);
    std::free(m->model_identifier);
    std::free(m->configuration);
    m->name = nullptr;
    m->description = nullptr;
    m->backend = nullptr;
    m->base_url = nullptr;
    m->model_identifier = nullptr;
    m->configuration = nullptr;
    acta_db_model_free(m);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, title(),
            tr("Could not save %1: %2").arg(noun())
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
    }
    return rc;
}

void ModelDialog::showRevision(QTreeWidgetItem *item)
{
    if (mode() != Mode::ReadOnly)
        return;

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
    ui->revisionLineEdit->setText(QString::number(rev->revision));

    // Configuration
    ui->baseUrlLineEdit->setText(utf8(rev->base_url));
    ui->backendTextEdit->setPlainText(utf8(rev->backend));
    ui->configurationTextEdit->setPlainText(utf8(rev->configuration));

    // Metadata
    ui->creationDateTimeEdit->setDateTime(toDateTime(rev->created_at));
    ui->updateDateTimeEdit->setDateTime(toDateTime(rev->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(rev->deleted_at));

    acta_db_model_revision_free(rev);
}
