#include "skillDialog.h"
#include "ui_skillDialog.h"

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

SkillDialog::SkillDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_skillDialog)
{
    ui->setupUi(this);
    setWindowTitle("Skill");
    // connect your buttons, validators, etc. here
}

SkillDialog::~SkillDialog()
{
    delete ui;
}

void SkillDialog::editSkill(db_t *db, int skillId)
{
    int err = ACTA_DB_OK;
    skill_t *s = acta_db_skill_get(db, skillId, &err);
    if (!s) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_get(%d) failed: %s", skillId,
                     acta_db_strerror(err));
        return;
    }

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

    // Revision (latest snapshot for this skill).
    err = ACTA_DB_OK;
    skill_revision_t *rev =
        acta_db_skill_revision_get_latest(db, skillId, &err);
    if (rev) {
        ui->revisionLineEdit->setText(QString::number(rev->revision));
        acta_db_skill_revision_free(rev);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_skill_revision_get_latest(%d) failed: %s", skillId,
                 acta_db_strerror(err));
    }

    acta_db_skill_free(s);
}
