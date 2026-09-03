#include "executionCreateDialog.h"

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>

#include <QtGlobal>

#include <cstdlib>

#include "util.h"

ExecutionCreateDialog::ExecutionCreateDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_executionCreateDialog)
{
    ui->setupUi(this);
    setWindowTitle("New Execution");

    // This dialog only ever creates: Save | Close from the start.
    ui->buttonBox->setStandardButtons(
        QDialogButtonBox::StandardButton::Save |
            QDialogButtonBox::StandardButton::Close);
    // A QDialogButtonBox added by the .ui file is not wired to
    // accept/reject by QDialog (that only happens via
    // QDialog::setButtons), so connect both explicitly.
    // setStandardButtons() recreates the button widgets, hence the
    // connects go after it.
    connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Save),
            &QPushButton::clicked, this,
            &ExecutionCreateDialog::onSaveClicked);
    connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Close),
            &QPushButton::clicked, this, &QDialog::reject);

    // Two-stage selection: picking an entity populates its revision
    // combo; the revision combo carries the *_revision_id the DB
    // needs. The revision listers return rows ascending, so the last
    // entry is the latest revision and gets preselected.
    connect(ui->skillComboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { loadSkillRevisions(); });
    connect(ui->modelComboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { loadModelRevisions(); });

    // Save stays disabled until every required field is set.
    connect(ui->contextComboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { updateSaveEnabled(); });
    connect(ui->skillComboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { updateSaveEnabled(); });
    connect(ui->skillRevisionComboBox, &QComboBox::currentIndexChanged,
            this, [this](int) { updateSaveEnabled(); });
    connect(ui->modelComboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { updateSaveEnabled(); });
    connect(ui->modelRevisionComboBox, &QComboBox::currentIndexChanged,
            this, [this](int) { updateSaveEnabled(); });
    connect(ui->promptTextEdit, &QTextEdit::textChanged, this,
            [this] { updateSaveEnabled(); });
}

ExecutionCreateDialog::~ExecutionCreateDialog()
{
    delete ui;
}

void ExecutionCreateDialog::newExecution(db_t *db)
{
    m_db = db;
    m_saved = false;
    m_newId = 0;

    ui->promptTextEdit->clear();
    loadContexts();
    loadSkills();
    loadModels();
    loadParentExecutions();

    updateSaveEnabled();
}

void ExecutionCreateDialog::loadContexts()
{
    ui->contextComboBox->clear();
    if (!m_db)
        return;

    int n = 0;
    int err = ACTA_DB_OK;
    context_t **contexts =
        acta_db_context_query(m_db, nullptr, 0, 0, &n, &err);
    if (!contexts) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_query failed: %s", acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        // Contexts have no name column; the type is the identifying
        // label (as in the context panel).
        const QString type = QString::fromUtf8(contexts[i]->type);
        ui->contextComboBox->addItem(type.isEmpty()
                                          ? QStringLiteral("context")
                                          : type,
                                      contexts[i]->id);
    }
    acta_db_context_list_free(contexts, n);
}

void ExecutionCreateDialog::loadSkills()
{
    ui->skillComboBox->clear();
    if (!m_db)
        return;

    int n = 0;
    int err = ACTA_DB_OK;
    skill_t **skills = acta_db_skill_list_all(m_db, 0, 0, &n, &err);
    if (!skills) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_list_all failed: %s",
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i)
        ui->skillComboBox->addItem(
            QString::fromUtf8(skills[i]->name), skills[i]->id);
    acta_db_skill_list_free(skills, n);
}

void ExecutionCreateDialog::loadSkillRevisions()
{
    ui->skillRevisionComboBox->clear();
    const int skillId = ui->skillComboBox->currentData().toInt();
    if (skillId == 0 || !m_db)
        return;

    int n = 0;
    int err = ACTA_DB_OK;
    skill_revision_t **revs =
        acta_db_skill_revision_list_by_skill(m_db, skillId, 0, 0, &n, &err);
    if (!revs) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_skill_revision_list_by_skill(%d) failed: %s",
                     skillId, acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i)
        ui->skillRevisionComboBox->addItem(
            QStringLiteral("rev %1").arg(revs[i]->revision), revs[i]->id);
    acta_db_skill_revision_list_free(revs, n);

    // Ascending list: preselect the latest revision.
    ui->skillRevisionComboBox->setCurrentIndex(
        ui->skillRevisionComboBox->count() - 1);
}

void ExecutionCreateDialog::loadModels()
{
    ui->modelComboBox->clear();
    if (!m_db)
        return;

    int n = 0;
    int err = ACTA_DB_OK;
    model_t **models = acta_db_model_list_all(m_db, 0, 0, &n, &err);
    if (!models) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_list_all failed: %s",
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i)
        ui->modelComboBox->addItem(
            QString::fromUtf8(models[i]->name), models[i]->id);
    acta_db_model_list_free(models, n);
}

void ExecutionCreateDialog::loadModelRevisions()
{
    ui->modelRevisionComboBox->clear();
    const int modelId = ui->modelComboBox->currentData().toInt();
    if (modelId == 0 || !m_db)
        return;

    int n = 0;
    int err = ACTA_DB_OK;
    model_revision_t **revs =
        acta_db_model_revision_list_by_model(m_db, modelId, 0, 0, &n, &err);
    if (!revs) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_model_revision_list_by_model(%d) failed: %s",
                     modelId, acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i)
        ui->modelRevisionComboBox->addItem(
            QStringLiteral("rev %1").arg(revs[i]->revision), revs[i]->id);
    acta_db_model_revision_list_free(revs, n);

    // Ascending list: preselect the latest revision.
    ui->modelRevisionComboBox->setCurrentIndex(
        ui->modelRevisionComboBox->count() - 1);
}

void ExecutionCreateDialog::loadParentExecutions()
{
    ui->parentExecutionComboBox->clear();
    // "— none —" maps to parent_execution_id = 0 (root execution);
    // default selection.
    ui->parentExecutionComboBox->addItem(QStringLiteral("— none —"), 0);
    if (!m_db)
        return;

    int n = 0;
    int err = ACTA_DB_OK;
    execution_t **executions =
        acta_db_execution_query(m_db, nullptr, 0, 0, &n, &err);
    if (!executions) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_query failed: %s",
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i)
        ui->parentExecutionComboBox->addItem(
            QStringLiteral("execution %1 (%2)")
                .arg(executions[i]->id)
                .arg(executions[i]->status
                           ? QString::fromUtf8(executions[i]->status)
                           : QString()),
            executions[i]->id);
    acta_db_execution_list_free(executions, n);
}

void ExecutionCreateDialog::updateSaveEnabled()
{
    const bool ready =
        ui->contextComboBox->currentData().toInt() != 0
        && ui->skillComboBox->currentData().toInt() != 0
        && ui->skillRevisionComboBox->currentData().toInt() != 0
        && ui->modelComboBox->currentData().toInt() != 0
        && ui->modelRevisionComboBox->currentData().toInt() != 0
        && !ui->promptTextEdit->toPlainText().trimmed().isEmpty();
    if (QPushButton *save =
            ui->buttonBox->button(QDialogButtonBox::StandardButton::Save))
        save->setEnabled(ready);
}

void ExecutionCreateDialog::onSaveClicked()
{
    if (!m_db || m_saved)
        return;

    const QString prompt = ui->promptTextEdit->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "New Execution",
                             tr("The prompt is required."));
        return;
    }

    execution_t e{};
    e.context_id = ui->contextComboBox->currentData().toInt();
    e.skill_revision_id =
        ui->skillRevisionComboBox->currentData().toInt();
    e.model_revision_id =
        ui->modelRevisionComboBox->currentData().toInt();
    e.parent_execution_id =
        ui->parentExecutionComboBox->currentData().toInt();
    e.prompt = dupString(prompt.toUtf8().constData());

    int newId = 0;
    int rc = acta_db_execution_create(m_db, &e, &newId);
    std::free(e.prompt);
    if (rc != ACTA_DB_OK) {
        // Defensive: the dropdowns come from live queries, so an FK
        // failure is unlikely; INVALID/FK both mean the referenced
        // row is gone. Everything else surfaces the raw error.
        const QString message =
            (rc == ACTA_DB_ERR_INVALID || rc == ACTA_DB_ERR_FK)
                ? tr("The selected context, skill or model no longer "
                      "exists.")
                : tr("Could not create the execution: %1")
                      .arg(QString::fromUtf8(acta_db_strerror(rc)));
        QMessageBox::warning(this, "New Execution", message);
        return;
    }

    // Persisted; report it so the panel reloads only on success
    // (UR #8).
    m_saved = true;
    m_newId = newId;
}
