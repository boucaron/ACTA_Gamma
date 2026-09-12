#include "skillDialog.h"
#include "ui_skillDialog.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QtGlobal>

#include "util.h"

SkillDialog::SkillDialog(QWidget *parent)
    : EntityDialog(QStringLiteral("Skill"), QStringLiteral("skill"), parent),
      ui(new Ui_skillDialog)
{
    ui->setupUi(this);
    configureRevisionTree();
    ui->nameLineEdit->setPlaceholderText(tr("Name…"));

    setMode(Mode::ReadOnly);
}

SkillDialog::~SkillDialog()
{
    delete ui;
}

void SkillDialog::applyReadOnly(bool readOnly)
{
    ui->nameLineEdit->setReadOnly(readOnly);
    ui->descriptionTextEdit->setReadOnly(readOnly);
    ui->promptTextEdit->setReadOnly(readOnly);
    ui->outputSchemaTextEdit->setReadOnly(readOnly);
}

QString SkillDialog::entityName() const
{
    return ui->nameLineEdit->text();
}

void SkillDialog::newSkill(db_t *db, int folderId)
{
    setDb(db);
    setEntityId(0);
    setFolderId(folderId);
    setSaved(false);

    // Resolve the target folder name for the window title (UR #34);
    // 0 means the root level, in which case the title says so.
    if (folderId > 0) {
        int err = ACTA_DB_OK;
        skill_folder_t *f = acta_db_skill_folder_get(db, folderId, &err);
        if (f) {
            setTargetFolder(QString::fromUtf8(f->name));
            acta_db_skill_folder_free(f);
        }
    }

    ui->nameLineEdit->clear();
    ui->descriptionTextEdit->clear();
    ui->revisionLineEdit->clear();
    ui->promptTextEdit->clear();
    ui->outputSchemaTextEdit->clear();
    ui->creationDateTimeEdit->setDateTime(QDateTime());
    ui->updateDateTimeEdit->setDateTime(QDateTime());
    ui->deleteDateTimeEdit->setDateTime(QDateTime());
    ui->revisionTreeWidget->clear();

    setMode(Mode::New);
    setWindowTitle(newTitle());
}

void SkillDialog::showSkill(db_t *db, int skillId)
{
    setDb(db);
    loadEntity(skillId);
    setMode(Mode::ReadOnly);
    setWindowTitle(showTitle());
}

void SkillDialog::editSkill(db_t *db, int skillId)
{
    setDb(db);
    setSaved(false);
    loadEntity(skillId);
    setMode(Mode::Edit);
    setWindowTitle(editTitle());
}

void SkillDialog::loadEntity(int skillId)
{
    int err = ACTA_DB_OK;
    skill_t *s = acta_db_skill_get(m_db, skillId, &err);
    if (!s) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_get(%d) failed: %s", skillId,
                     acta_db_strerror(err));
        return;
    }

    m_id = s->id;
    m_folderId = s->folder_id;

    // Details
    ui->nameLineEdit->setText(utf8(s->name));
    ui->descriptionTextEdit->setPlainText(utf8(s->description));

    // Prompt / output schema
    ui->promptTextEdit->setPlainText(utf8(s->prompt_template));
    ui->outputSchemaTextEdit->setPlainText(utf8(s->output_schema));

    // Metadata (locale-formatted by QDateTimeEdit; the exact ISO value
    // with seconds lives in the tooltips, UR #24).
    ui->creationDateTimeEdit->setDateTime(toDateTime(s->created_at));
    ui->creationDateTimeEdit->setToolTip(utf8(s->created_at));
    ui->updateDateTimeEdit->setDateTime(toDateTime(s->updated_at));
    ui->updateDateTimeEdit->setToolTip(utf8(s->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(s->deleted_at));
    ui->deleteDateTimeEdit->setToolTip(utf8(s->deleted_at));

    // Revisions: one row per snapshot, ascending (oldest first).
    loadRevisionTree([this](int id, int &e) {
        int n = 0;
        skill_revision_t **revs =
            acta_db_skill_revision_list_by_skill(m_db, id, 0, 0, &n, &e);
        QList<RevisionRow> out;
        if (revs) {
            for (int i = 0; i < n; ++i)
                out.append(RevisionRow{revs[i]->id, revs[i]->revision});
            acta_db_skill_revision_list_free(revs, n);
        }
        return out;
    });

    acta_db_skill_free(s);
}

bool SkillDialog::validateFields()
{
    const QString name = ui->nameLineEdit->text().trimmed();
    const QString prompt = ui->promptTextEdit->toPlainText();
    if (name.isEmpty() || prompt.isEmpty()) {
        QMessageBox::warning(this, title(),
                             tr("Name and prompt are required."));
        return false;
    }
    const QString schema = ui->outputSchemaTextEdit->toPlainText();
    if (!schema.isEmpty()) {
        // R5: output_schema must be a valid JSON object when present.
        QJsonParseError perr;
        const QJsonDocument doc =
            QJsonDocument::fromJson(schema.toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError) {
            // QJsonParseError only exposes a byte offset; derive the
            // line/column from the schema text up to that offset.
            const QByteArray data = schema.toUtf8();
            int line = 1, col = 1;
            for (int i = 0; i < perr.offset && i < data.size(); ++i) {
                if (data[i] == '\n') {
                    ++line;
                    col = 1;
                } else {
                    ++col;
                }
            }
            QMessageBox::warning(
                this, title(),
                tr("Invalid JSON in output schema: %1 (line %2, column %3)")
                    .arg(perr.errorString())
                    .arg(line)
                    .arg(col));
            return false;
        }
        if (!doc.isObject()) {
            QMessageBox::warning(this, title(),
                                 tr("Output schema must be a JSON object."));
            return false;
        }
    }
    return true;
}

int SkillDialog::createEntity(int *outId)
{
    const QByteArray nameBa = ui->nameLineEdit->text().trimmed().toUtf8();
    const QByteArray descriptionBa =
        ui->descriptionTextEdit->toPlainText().toUtf8();
    const QByteArray promptBa = ui->promptTextEdit->toPlainText().toUtf8();
    const QByteArray outputSchemaBa =
        ui->outputSchemaTextEdit->toPlainText().toUtf8();

    skill_t s{};
    s.folder_id = m_folderId;
    s.name = dupString(nameBa.constData());
    s.description =
        dupString(descriptionBa.isEmpty() ? nullptr
                                           : descriptionBa.constData());
    s.prompt_template = dupString(promptBa.constData());
    s.output_schema =
        dupString(outputSchemaBa.isEmpty() ? nullptr
                                            : outputSchemaBa.constData());

    const int rc = acta_db_skill_create(m_db, &s, outId);
    std::free(s.name);
    std::free(s.description);
    std::free(s.prompt_template);
    std::free(s.output_schema);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, title(),
            friendlyDbError(rc, noun(),
                            ui->nameLineEdit->text().trimmed(),
                            tr("Could not create %1: %2"),
                            acta_db_last_error(m_db)));
    }
    return rc;
}

int SkillDialog::updateEntity()
{
    const QByteArray nameBa = ui->nameLineEdit->text().trimmed().toUtf8();
    const QByteArray descriptionBa =
        ui->descriptionTextEdit->toPlainText().toUtf8();
    const QByteArray promptBa = ui->promptTextEdit->toPlainText().toUtf8();
    const QByteArray outputSchemaBa =
        ui->outputSchemaTextEdit->toPlainText().toUtf8();

    // Full-row update: fetch the live row, replace the editable
    // fields, write it back.
    int err = ACTA_DB_OK;
    skill_t *s = acta_db_skill_get_live(m_db, m_id, &err);
    if (!s) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_get_live(%d) failed: %s", m_id,
                     acta_db_strerror(err));
        return err != ACTA_DB_OK ? err : ACTA_DB_ERR_INVALID;
    }
    std::free(s->name);
    std::free(s->description);
    std::free(s->prompt_template);
    std::free(s->output_schema);
    s->name = dupString(nameBa.constData());
    s->description =
        dupString(descriptionBa.isEmpty() ? nullptr
                                           : descriptionBa.constData());
    s->prompt_template = dupString(promptBa.constData());
    s->output_schema =
        dupString(outputSchemaBa.isEmpty() ? nullptr
                                            : outputSchemaBa.constData());

    const int rc = acta_db_skill_update(m_db, s);
    // We own these duplicates now; null them so acta_db_skill_free
    // only releases what it allocated.
    std::free(s->name);
    std::free(s->description);
    std::free(s->prompt_template);
    std::free(s->output_schema);
    s->name = nullptr;
    s->description = nullptr;
    s->prompt_template = nullptr;
    s->output_schema = nullptr;
    acta_db_skill_free(s);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, title(),
            friendlyDbError(rc, noun(),
                            ui->nameLineEdit->text().trimmed(),
                            tr("Could not save %1: %2"),
                            acta_db_last_error(m_db)));
    }
    return rc;
}

void SkillDialog::showRevision(QTreeWidgetItem *item)
{
    if (mode() != Mode::ReadOnly)
        return;

    const int revisionId =
        item ? item->data(0, RoleRevisionId).toInt() : 0;
    if (revisionId == 0 || !m_db)
        return;

    int err = ACTA_DB_OK;
    skill_revision_t *rev = acta_db_skill_revision_get(m_db, revisionId, &err);
    if (!rev) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_revision_get(%d) failed: %s", revisionId,
                     acta_db_strerror(err));
        return;
    }

    // Details
    ui->nameLineEdit->setText(utf8(rev->name));
    ui->descriptionTextEdit->setPlainText(utf8(rev->description));
    ui->revisionLineEdit->setText(QString::number(rev->revision));

    // Prompt / output schema
    ui->promptTextEdit->setPlainText(utf8(rev->prompt_template));
    ui->outputSchemaTextEdit->setPlainText(utf8(rev->output_schema));

    // Metadata
    ui->creationDateTimeEdit->setDateTime(toDateTime(rev->created_at));
    ui->creationDateTimeEdit->setToolTip(utf8(rev->created_at));
    ui->updateDateTimeEdit->setDateTime(toDateTime(rev->updated_at));
    ui->updateDateTimeEdit->setToolTip(utf8(rev->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(rev->deleted_at));
    ui->deleteDateTimeEdit->setToolTip(utf8(rev->deleted_at));

    acta_db_skill_revision_free(rev);
}
