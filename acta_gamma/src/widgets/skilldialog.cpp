#include "skillDialog.h"
#include "ui_skillDialog.h"

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

SkillDialog::SkillDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_skillDialog)
{
    ui->setupUi(this);
    setWindowTitle("Skill");

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

SkillDialog::~SkillDialog()
{
    delete ui;
}

void SkillDialog::setMode(Mode mode)
{
    m_mode = mode;
    const bool readOnly = (mode == Mode::ReadOnly);

    ui->buttonBox->setStandardButtons(
        readOnly ? QDialogButtonBox::StandardButton::Close
                 : QDialogButtonBox::StandardButton::Save |
                       QDialogButtonBox::StandardButton::Close);
    if (!readOnly) {
        connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Save),
                &QPushButton::clicked, this, &SkillDialog::onSaveClicked);
    }

    ui->nameLineEdit->setReadOnly(readOnly);
    ui->descriptionTextEdit->setReadOnly(readOnly);
    ui->promptTextEdit->setReadOnly(readOnly);
    ui->outputSchemaTextEdit->setReadOnly(readOnly);
    // Disable revision switching outside Read-only mode: clicking a
    // revision overwrites the form fields, which would clobber unsaved
    // edits in New/Edit mode.
    ui->revisionTreeWidget->setEnabled(readOnly);
}

void SkillDialog::newSkill(db_t *db, int folderId)
{
    m_db = db;
    m_skillId = 0;
    m_folderId = folderId;

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
    setWindowTitle("New Skill");
}

void SkillDialog::showSkill(db_t *db, int skillId)
{
    m_db = db;
    loadSkill(skillId);
    setMode(Mode::ReadOnly);
    setWindowTitle(
        QStringLiteral("Skill: %1").arg(ui->nameLineEdit->text()));
}

void SkillDialog::editSkill(db_t *db, int skillId)
{
    m_db = db;
    loadSkill(skillId);
    setMode(Mode::Edit);
    setWindowTitle(
        QStringLiteral("Edit Skill: %1").arg(ui->nameLineEdit->text()));
}

void SkillDialog::loadSkill(int skillId)
{
    int err = ACTA_DB_OK;
    skill_t *s = acta_db_skill_get(m_db, skillId, &err);
    if (!s) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_get(%d) failed: %s", skillId,
                     acta_db_strerror(err));
        return;
    }

    m_skillId = s->id;
    m_folderId = s->folder_id;

    // Details
    ui->nameLineEdit->setText(utf8(s->name));
    ui->descriptionTextEdit->setPlainText(utf8(s->description));

    // Prompt / output schema
    ui->promptTextEdit->setPlainText(utf8(s->prompt_template));
    ui->outputSchemaTextEdit->setPlainText(utf8(s->output_schema));

    // Metadata
    ui->creationDateTimeEdit->setDateTime(toDateTime(s->created_at));
    ui->updateDateTimeEdit->setDateTime(toDateTime(s->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(s->deleted_at));

    // Revisions: one row per snapshot, ascending (oldest first).
    err = ACTA_DB_OK;
    int nRevs = 0;
    skill_revision_t **revs =
        acta_db_skill_revision_list_by_skill(m_db, skillId, 0, 0, &nRevs,
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
        acta_db_skill_revision_list_free(revs, nRevs);
        // Select the latest (last, list is ascending) to show it.
        if (nRevs > 0)
            ui->revisionTreeWidget->setCurrentItem(
                ui->revisionTreeWidget->topLevelItem(nRevs - 1));
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_skill_revision_list_by_skill(%d) failed: %s",
                 skillId, acta_db_strerror(err));
    }

    acta_db_skill_free(s);
}

void SkillDialog::onSaveClicked()
{
    if (!m_db)
        return;

    const QString name = ui->nameLineEdit->text().trimmed();
    const QString prompt = ui->promptTextEdit->toPlainText();
    if (name.isEmpty() || prompt.isEmpty()) {
        QMessageBox::warning(this, "Skill",
                             "Name and prompt are required.");
        return;
    }

    const QByteArray nameBa = name.toUtf8();
    const QByteArray descriptionBa =
        ui->descriptionTextEdit->toPlainText().toUtf8();
    const QByteArray promptBa = prompt.toUtf8();
    const QByteArray outputSchemaBa =
        ui->outputSchemaTextEdit->toPlainText().toUtf8();

    int err = ACTA_DB_OK;
    if (m_mode == Mode::New) {
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

        int newId = 0;
        int rc = acta_db_skill_create(m_db, &s, &newId);
        std::free(s.name);
        std::free(s.description);
        std::free(s.prompt_template);
        std::free(s.output_schema);
        if (rc != ACTA_DB_OK) {
            QMessageBox::warning(
                this, "Skill",
                QStringLiteral("Could not create skill: %1")
                    .arg(acta_db_strerror(rc)));
            return;
        }

        // Keep the dialog open on the created row, now editable.
        m_skillId = newId;
        loadSkill(newId);
        setMode(Mode::Edit);
        setWindowTitle(
            QStringLiteral("Edit Skill: %1").arg(ui->nameLineEdit->text()));
        QMessageBox::information(this, "Skill", "Skill saved.");
    } else if (m_mode == Mode::Edit && m_skillId != 0) {
        // Full-row update: fetch the live row, replace the editable
        // fields, write it back.
        skill_t *s = acta_db_skill_get_live(m_db, m_skillId, &err);
        if (!s) {
            if (err != ACTA_DB_OK)
                qWarning("acta_db_skill_get_live(%d) failed: %s", m_skillId,
                         acta_db_strerror(err));
            return;
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
                this, "Skill",
                QStringLiteral("Could not save skill: %1")
                    .arg(acta_db_strerror(rc)));
            return;
        }

        // The update triggered a revision snapshot: reload and switch
        // back to the read-only view. The "Saved as revision N" feedback
        // explains why editing stopped (the dialog is now read-only).
        loadSkill(m_skillId);
        setMode(Mode::ReadOnly);
        setWindowTitle(
            QStringLiteral("Skill: %1").arg(ui->nameLineEdit->text()));
        QMessageBox::information(
            this, "Skill",
            QStringLiteral("Saved as revision %1.").arg(m_latestRevision));
    }
}

void SkillDialog::showRevision(QTreeWidgetItem *item)
{
    if (m_mode != Mode::ReadOnly)
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
    ui->updateDateTimeEdit->setDateTime(toDateTime(rev->updated_at));
    ui->deleteDateTimeEdit->setDateTime(toDateTime(rev->deleted_at));

    acta_db_skill_revision_free(rev);
}
