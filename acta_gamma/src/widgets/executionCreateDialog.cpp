#include "executionCreateDialog.h"

#include <QComboBox>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QtGlobal>

#include <cstdlib>
#include <functional>

#include "util.h"

namespace {
const int RoleFolderId = Qt::UserRole;
const int RoleEntityId = Qt::UserRole + 1;
const int RoleRevisionId = Qt::UserRole + 2;

// One row of an entity folder tree (folder: parentId = parent_id;
// entity: parentId = folder_id; revision: sub = revision number).
struct Row {
    int id;
    int parentId;
    int sub;
    QString name;
};

// Folder tree with entity and revision rows, same layout as
// FolderTreePanel:
//   <folder>                  (nested via parent_id, folder icon)
//     <entity>
//       rev 1
//       rev 2   (listers are ascending; last = latest)
//   <root-level entity>       (folder_id = 0)
void buildEntityTree(QTreeWidget *tree, const QList<Row> &folders,
                     const QList<Row> &entities,
                     const std::function<QList<Row>(int)> &listRevisions)
{
    tree->clear();
    const QIcon folderIcon =
        tree->style()->standardIcon(QStyle::SP_DirIcon);

    // Folder skeleton: all folders in one call, nested via parent_id
    // (0 = root), exactly as FolderTreePanel::reload().
    QHash<int, QString> names;
    QHash<int, QList<int>> children;
    QList<int> roots;
    for (const Row &f : folders) {
        names.insert(f.id, f.name);
        if (f.parentId == 0)
            roots.append(f.id);
        else
            children[f.parentId].append(f.id);
    }

    QHash<int, QTreeWidgetItem *> folderItems;
    std::function<void(int, QTreeWidgetItem *)> addFolder;
    addFolder = [&](int id, QTreeWidgetItem *parent) {
        if (folderItems.contains(id))
            return; // defensive: data with a parent cycle
        auto *item = parent ? new QTreeWidgetItem(parent, {names.value(id)})
                            : new QTreeWidgetItem(tree, {names.value(id)});
        item->setIcon(0, folderIcon);
        item->setData(0, RoleFolderId, id);
        folderItems.insert(id, item);
        for (int kid : children.value(id))
            addFolder(kid, item);
    };
    for (int id : roots)
        addFolder(id, nullptr);

    // Entities: one list_all-style query, grouped by folder in C++
    // (UR #8 / P8a); revisions become the entity's child rows.
    QHash<int, QList<Row>> byFolder;
    for (const Row &e : entities)
        byFolder[e.parentId].append(e);

    auto addEntity = [&](QTreeWidgetItem *parent, const Row &e) {
        auto *item = parent ? new QTreeWidgetItem(parent, {e.name})
                            : new QTreeWidgetItem(tree, {e.name});
        item->setData(0, RoleEntityId, e.id);
        const QList<Row> revs = listRevisions(e.id);
        for (const Row &r : revs) {
            auto *rItem = new QTreeWidgetItem(
                item, {QStringLiteral("rev %1").arg(r.sub)});
            rItem->setData(0, RoleRevisionId, r.id);
        }
    };
    for (const Row &e : byFolder.value(0))
        addEntity(nullptr, e);
    for (auto it = folderItems.cbegin(); it != folderItems.cend(); ++it)
        for (const Row &e : byFolder.value(it.key()))
            addEntity(it.value(), e);

    // Fully expanded (the dialog is a picker, no need to fold).
    std::function<void(QTreeWidgetItem *)> expand;
    expand = [&](QTreeWidgetItem *item) {
        item->setExpanded(true);
        for (int i = 0; i < item->childCount(); ++i)
            expand(item->child(i));
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        expand(tree->topLevelItem(i));
}

// Select the first entity in tree order; the selection handler
// auto-selects its latest revision (last child).
void selectFirstEntity(QTreeWidget *tree)
{
    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> find;
    find = [&](QTreeWidgetItem *item) -> QTreeWidgetItem * {
        if (item->data(0, RoleEntityId).toInt() != 0)
            return item;
        for (int i = 0; i < item->childCount(); ++i)
            if (QTreeWidgetItem *hit = find(item->child(i)))
                return hit;
        return nullptr;
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem *entity = find(tree->topLevelItem(i))) {
            tree->setCurrentItem(entity);
            return;
        }
    }
}
} // namespace

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

    // Tree selection: a revision row picks its id, an entity row
    // auto-selects its latest revision. The revision id lives in
    // m_skillRevisionId / m_modelRevisionId (0 = nothing selectable).
    connect(ui->skillTreeWidget, &QTreeWidget::currentItemChanged, this,
            &ExecutionCreateDialog::onSkillTreeSelectionChanged);
    connect(ui->modelTreeWidget, &QTreeWidget::currentItemChanged, this,
            &ExecutionCreateDialog::onModelTreeSelectionChanged);

    // Save stays disabled until every required field is set.
    connect(ui->contextComboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { updateSaveEnabled(); });
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
    m_skillRevisionId = 0;
    m_modelRevisionId = 0;

    ui->promptTextEdit->clear();
    loadContexts();
    loadSkillTree();
    loadModelTree();
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
        // Contexts have no name column; type + creation date identify
        // the row (the same columns as the context panel list).
        const QString type = QString::fromUtf8(contexts[i]->type);
        const QDateTime created = toDateTime(contexts[i]->created_at);
        const QString label =
            (type.isEmpty() ? QStringLiteral("context") : type)
                + (created.isValid()
                       ? QStringLiteral(" (%1)").arg(
                             created.toString("yyyy-MM-dd"))
                       : QString());
        // The content is the main identifying data of a context; the
        // tooltip shows it (truncated) since the list row cannot.
        const QString content = QString::fromUtf8(contexts[i]->content);
        ui->contextComboBox->addItem(label, contexts[i]->id);
        ui->contextComboBox->setItemData(
            ui->contextComboBox->count() - 1,
            content.size() > 400 ? content.left(400) + QStringLiteral("…")
                                 : content,
            Qt::ToolTipRole);
    }
    acta_db_context_list_free(contexts, n);
}

void ExecutionCreateDialog::loadSkillTree()
{
    QTreeWidget *tree = ui->skillTreeWidget;
    m_skillRevisionId = 0;
    if (!m_db) {
        tree->clear();
        return;
    }

    int n = 0;
    int err = ACTA_DB_OK;
    QList<Row> folders;
    skill_folder_t **f = acta_db_skill_folder_list_all(m_db, 0, 0, &n, &err);
    if (f) {
        for (int i = 0; i < n; ++i)
            folders.append(Row{f[i]->id, f[i]->parent_id, 0,
                               QString::fromUtf8(f[i]->name)});
        acta_db_skill_folder_list_free(f, n);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_skill_folder_list_all failed: %s",
                 acta_db_strerror(err));
    }

    QList<Row> entities;
    n = 0;
    err = ACTA_DB_OK;
    skill_t **s = acta_db_skill_list_all(m_db, 0, 0, &n, &err);
    if (s) {
        for (int i = 0; i < n; ++i)
            entities.append(Row{s[i]->id, s[i]->folder_id, 0,
                                QString::fromUtf8(s[i]->name)});
        acta_db_skill_list_free(s, n);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_skill_list_all failed: %s", acta_db_strerror(err));
    }

    buildEntityTree(tree, folders, entities, [this](int skillId) {
        int n = 0;
        int err = ACTA_DB_OK;
        QList<Row> revs;
        skill_revision_t **rev =
            acta_db_skill_revision_list_by_skill(m_db, skillId, 0, 0, &n, &err);
        if (rev) {
            for (int i = 0; i < n; ++i)
                revs.append(Row{rev[i]->id, 0, rev[i]->revision, QString()});
            acta_db_skill_revision_list_free(rev, n);
        } else if (err != ACTA_DB_OK) {
            qWarning("acta_db_skill_revision_list_by_skill(%d) failed: %s",
                     skillId, acta_db_strerror(err));
        }
        return revs;
    });

    // Default: the first skill's latest revision (the selection
    // handler auto-selects the last child), mirroring the old
    // two-stage combos' "latest preselected".
    selectFirstEntity(tree);
}

void ExecutionCreateDialog::loadModelTree()
{
    QTreeWidget *tree = ui->modelTreeWidget;
    m_modelRevisionId = 0;
    if (!m_db) {
        tree->clear();
        return;
    }

    int n = 0;
    int err = ACTA_DB_OK;
    QList<Row> folders;
    model_folder_t **f = acta_db_model_folder_list_all(m_db, 0, 0, &n, &err);
    if (f) {
        for (int i = 0; i < n; ++i)
            folders.append(Row{f[i]->id, f[i]->parent_id, 0,
                               QString::fromUtf8(f[i]->name)});
        acta_db_model_folder_list_free(f, n);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_model_folder_list_all failed: %s",
                 acta_db_strerror(err));
    }

    QList<Row> entities;
    n = 0;
    err = ACTA_DB_OK;
    model_t **m = acta_db_model_list_all(m_db, 0, 0, &n, &err);
    if (m) {
        for (int i = 0; i < n; ++i)
            entities.append(Row{m[i]->id, m[i]->folder_id, 0,
                                QString::fromUtf8(m[i]->name)});
        acta_db_model_list_free(m, n);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_model_list_all failed: %s", acta_db_strerror(err));
    }

    buildEntityTree(tree, folders, entities, [this](int modelId) {
        int n = 0;
        int err = ACTA_DB_OK;
        QList<Row> revs;
        model_revision_t **rev =
            acta_db_model_revision_list_by_model(m_db, modelId, 0, 0, &n, &err);
        if (rev) {
            for (int i = 0; i < n; ++i)
                revs.append(Row{rev[i]->id, 0, rev[i]->revision, QString()});
            acta_db_model_revision_list_free(rev, n);
        } else if (err != ACTA_DB_OK) {
            qWarning("acta_db_model_revision_list_by_model(%d) failed: %s",
                     modelId, acta_db_strerror(err));
        }
        return revs;
    });

    // Default: the first model's latest revision.
    selectFirstEntity(tree);
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
        && m_skillRevisionId != 0
        && m_modelRevisionId != 0
        && !ui->promptTextEdit->toPlainText().trimmed().isEmpty();
    if (QPushButton *save =
            ui->buttonBox->button(QDialogButtonBox::StandardButton::Save))
        save->setEnabled(ready);
}

void ExecutionCreateDialog::onSkillTreeSelectionChanged(QTreeWidgetItem *cur,
                                                        QTreeWidgetItem *)
{
    m_skillRevisionId = 0;
    if (cur) {
        if (cur->data(0, RoleRevisionId).toInt() != 0) {
            m_skillRevisionId = cur->data(0, RoleRevisionId).toInt();
        } else if (cur->data(0, RoleEntityId).toInt() != 0) {
            // Entity row: auto-select its latest revision (last child;
            // the lister is ascending). The signal fires again and
            // lands in the branch above.
            if (cur->childCount() > 0)
                ui->skillTreeWidget->setCurrentItem(
                    cur->child(cur->childCount() - 1));
        }
    }
    updateSaveEnabled();
}

void ExecutionCreateDialog::onModelTreeSelectionChanged(QTreeWidgetItem *cur,
                                                        QTreeWidgetItem *)
{
    m_modelRevisionId = 0;
    if (cur) {
        if (cur->data(0, RoleRevisionId).toInt() != 0) {
            m_modelRevisionId = cur->data(0, RoleRevisionId).toInt();
        } else if (cur->data(0, RoleEntityId).toInt() != 0) {
            // Entity row: auto-select its latest revision (last child;
            // the lister is ascending). The signal fires again and
            // lands in the branch above.
            if (cur->childCount() > 0)
                ui->modelTreeWidget->setCurrentItem(
                    cur->child(cur->childCount() - 1));
        }
    }
    updateSaveEnabled();
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
    e.skill_revision_id = m_skillRevisionId;
    e.model_revision_id = m_modelRevisionId;
    e.parent_execution_id =
        ui->parentExecutionComboBox->currentData().toInt();
    e.prompt = dupString(prompt.toUtf8().constData());

    int newId = 0;
    int rc = acta_db_execution_create(m_db, &e, &newId);
    std::free(e.prompt);
    if (rc != ACTA_DB_OK) {
        // Defensive: the form is filled from live queries, so an FK
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
