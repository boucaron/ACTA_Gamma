#include "modelDialog.h"
#include "ui_modelDialog.h"

#include <QDateTime>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QtGlobal>

#include <cstdlib>
#include <cstring>

// strdup is not part of the C standard; duplicate into a malloc block
// freed by free().
static char *dupString(const char *s)
{
    if (!s)
        return nullptr;
    const size_t n = std::strlen(s) + 1;
    char *copy = static_cast<char *>(std::malloc(n));
    if (copy)
        std::memcpy(copy, s, n);
    return copy;
}

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

    setMode(Mode::ReadOnly);
}

ModelDialog::~ModelDialog()
{
    delete ui;
}

void ModelDialog::setMode(Mode mode)
{
    m_mode = mode;
    const bool readOnly = (mode == Mode::ReadOnly);

    ui->buttonBox->setStandardButtons(
        readOnly ? QDialogButtonBox::StandardButton::Close
                 : QDialogButtonBox::StandardButton::Save |
                       QDialogButtonBox::StandardButton::Close);
    if (!readOnly) {
        connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Save),
                &QPushButton::clicked, this, &ModelDialog::onSaveClicked);
    }
    // A QDialogButtonBox added by the .ui file is not wired to
    // accept/reject by QDialog (that only happens via QDialog::setButtons),
    // so connect Close explicitly. setStandardButtons() recreates the
    // button widgets on every call, hence the connect goes after it.
    connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Close),
            &QPushButton::clicked, this, &QDialog::reject);

    ui->nameLineEdit->setReadOnly(readOnly);
    ui->descriptionTextEdit->setReadOnly(readOnly);
    ui->modelTextEdit->setReadOnly(readOnly);
    ui->baseUrlLineEdit->setReadOnly(readOnly);
    ui->backendTextEdit->setReadOnly(readOnly);
    ui->configurationTextEdit->setReadOnly(readOnly);
    // Disable revision switching outside Read-only mode: clicking a
    // revision overwrites the form fields, which would clobber unsaved
    // edits in New/Edit mode.
    ui->revisionTreeWidget->setEnabled(readOnly);
}

void ModelDialog::newModel(db_t *db, int folderId)
{
    m_db = db;
    m_modelId = 0;
    m_folderId = folderId;

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
    setWindowTitle("New Model");
}

void ModelDialog::showModel(db_t *db, int modelId)
{
    m_db = db;
    loadModel(modelId);
    setMode(Mode::ReadOnly);
    setWindowTitle(
        QStringLiteral("Model: %1").arg(ui->nameLineEdit->text()));
}

void ModelDialog::editModel(db_t *db, int modelId)
{
    m_db = db;
    loadModel(modelId);
    setMode(Mode::Edit);
    setWindowTitle(
        QStringLiteral("Edit Model: %1").arg(ui->nameLineEdit->text()));
}

void ModelDialog::loadModel(int modelId)
{
    int err = ACTA_DB_OK;
    model_t *m = acta_db_model_get(m_db, modelId, &err);
    if (!m) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_get(%d) failed: %s", modelId,
                     acta_db_strerror(err));
        return;
    }

    m_modelId = m->id;
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
    err = ACTA_DB_OK;
    int nRevs = 0;
    model_revision_t **revs =
        acta_db_model_revision_list_by_model(m_db, modelId, 0, 0, &nRevs,
                                             &err);
    ui->revisionTreeWidget->clear();
    if (revs) {
        for (int i = 0; i < nRevs; ++i) {
            auto *item = new QTreeWidgetItem(ui->revisionTreeWidget);
            item->setText(0, QStringLiteral("Revision %1")
                                   .arg(revs[i]->revision));
            item->setData(0, RoleRevisionId, revs[i]->id);
        }
        m_latestRevision = (nRevs > 0) ? revs[nRevs - 1]->revision : 0;
        acta_db_model_revision_list_free(revs, nRevs);
        // Select the latest (last, list is ascending) to show it.
        if (nRevs > 0)
            ui->revisionTreeWidget->setCurrentItem(
                ui->revisionTreeWidget->topLevelItem(nRevs - 1));
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_model_revision_list_by_model(%d) failed: %s",
                 modelId, acta_db_strerror(err));
    }

    acta_db_model_free(m);
}

void ModelDialog::onSaveClicked()
{
    if (!m_db)
        return;

    const QString name = ui->nameLineEdit->text().trimmed();
    const QString backend = ui->backendTextEdit->toPlainText().trimmed();
    const QString modelIdentifier =
        ui->modelTextEdit->toPlainText().trimmed();
    if (name.isEmpty() || backend.isEmpty() || modelIdentifier.isEmpty()) {
        QMessageBox::warning(this, "Model",
                             "Name, backend and model identifier are "
                             "required.");
        return;
    }

    const QByteArray nameBa = name.toUtf8();
    const QByteArray descriptionBa =
        ui->descriptionTextEdit->toPlainText().toUtf8();
    const QByteArray baseUrlBa =
        ui->baseUrlLineEdit->text().toUtf8();
    const QByteArray backendBa = backend.toUtf8();
    const QByteArray modelIdentifierBa = modelIdentifier.toUtf8();
    const QByteArray configurationBa =
        ui->configurationTextEdit->toPlainText().toUtf8();

    int err = ACTA_DB_OK;
    if (m_mode == Mode::New) {
        model_t m{};
        m.folder_id = m_folderId;
        m.name = dupString(nameBa.constData());
        m.description =
            dupString(descriptionBa.isEmpty() ? nullptr
                                              : descriptionBa.constData());
        m.backend = dupString(backendBa.constData());
        m.base_url =
            dupString(baseUrlBa.isEmpty() ? nullptr
                                          : baseUrlBa.constData());
        m.model_identifier = dupString(modelIdentifierBa.constData());
        m.configuration =
            dupString(configurationBa.isEmpty() ? nullptr
                                                 : configurationBa
                                                                 .constData());

        int newId = 0;
        int rc = acta_db_model_create(m_db, &m, &newId);
        std::free(m.name);
        std::free(m.description);
        std::free(m.backend);
        std::free(m.base_url);
        std::free(m.model_identifier);
        std::free(m.configuration);
        if (rc != ACTA_DB_OK) {
            QMessageBox::warning(
                this, "Model",
                QStringLiteral("Could not create model: %1")
                    .arg(acta_db_strerror(rc)));
            return;
        }

        // Keep the dialog open on the created row, now editable.
        m_modelId = newId;
        loadModel(newId);
        setMode(Mode::Edit);
        setWindowTitle(
            QStringLiteral("Edit Model: %1").arg(ui->nameLineEdit->text()));
        QMessageBox::information(this, "Model", "Model saved.");
    } else if (m_mode == Mode::Edit && m_modelId != 0) {
        // Full-row update: fetch the live row, replace the editable
        // fields, write it back.
        model_t *m = acta_db_model_get_live(m_db, m_modelId, &err);
        if (!m) {
            if (err != ACTA_DB_OK)
                qWarning("acta_db_model_get_live(%d) failed: %s", m_modelId,
                         acta_db_strerror(err));
            return;
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
            dupString(baseUrlBa.isEmpty() ? nullptr
                                          : baseUrlBa.constData());
        m->model_identifier = dupString(modelIdentifierBa.constData());
        m->configuration =
            dupString(configurationBa.isEmpty() ? nullptr
                                                 : configurationBa
                                                                 .constData());

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
                this, "Model",
                QStringLiteral("Could not save model: %1")
                    .arg(acta_db_strerror(rc)));
            return;
        }

        // The update triggered a revision snapshot: reload and switch
        // back to the read-only view. The "Saved as revision N" feedback
        // explains why editing stopped (the dialog is now read-only).
        loadModel(m_modelId);
        setMode(Mode::ReadOnly);
        setWindowTitle(
            QStringLiteral("Model: %1").arg(ui->nameLineEdit->text()));
        QMessageBox::information(
            this, "Model",
            QStringLiteral("Saved as revision %1.").arg(m_latestRevision));
    }
}

void ModelDialog::showRevision(QTreeWidgetItem *item)
{
    if (m_mode != Mode::ReadOnly)
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
