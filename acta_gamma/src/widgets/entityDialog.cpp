#include "entityDialog.h"

#include <QMessageBox>
#include <QTreeWidgetItem>

EntityDialog::EntityDialog(const QString &title, const QString &noun,
                           QWidget *parent)
    : QDialog(parent), m_title(title), m_noun(noun)
{
}

QString EntityDialog::newTitle() const
{
    // Show the target folder so the user sees where the new entity will
    // be created (UR #34).
    const QString where = m_targetFolder.isEmpty()
        ? tr("the root level")
        : tr("folder '%1'").arg(m_targetFolder);
    return tr("New %1 in %2").arg(m_title, where);
}

QString EntityDialog::showTitle() const
{
    return tr("%1: %2").arg(m_title, entityName());
}

QString EntityDialog::editTitle() const
{
    return tr("Edit %1: %2").arg(m_title, entityName());
}

QTreeWidget *EntityDialog::revisionTree() const
{
    return findChild<QTreeWidget *>(QStringLiteral("revisionTreeWidget"));
}

QDialogButtonBox *EntityDialog::buttonBox() const
{
    return findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
}

void EntityDialog::configureRevisionTree()
{
    auto *tree = revisionTree();
    tree->setColumnCount(1);
    tree->setHeaderLabel("Revision");
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    connect(tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showRevision(cur);
            });
}

void EntityDialog::setMode(Mode mode)
{
    m_mode = mode;
    const bool readOnly = (mode == Mode::ReadOnly);

    auto *box = buttonBox();
    box->setStandardButtons(
        readOnly ? QDialogButtonBox::StandardButton::Close
                 : QDialogButtonBox::StandardButton::Save |
                       QDialogButtonBox::StandardButton::Close);
    if (!readOnly) {
        connect(box->button(QDialogButtonBox::StandardButton::Save),
                &QPushButton::clicked, this, &EntityDialog::onSaveClicked);
    }
    // A QDialogButtonBox added by the .ui file is not wired to
    // accept/reject by QDialog (that only happens via
    // QDialog::setButtons), so connect Close explicitly.
    // setStandardButtons() recreates the button widgets on every call,
    // hence the connect goes after it.
    connect(box->button(QDialogButtonBox::StandardButton::Close),
            &QPushButton::clicked, this, &QDialog::reject);

    applyReadOnly(readOnly);

    // Disable revision switching outside Read-only mode: clicking a
    // revision overwrites the form fields, which would clobber unsaved
    // edits in New/Edit mode.
    revisionTree()->setEnabled(readOnly);
}

int EntityDialog::loadRevisionTree(
    const std::function<QList<RevisionRow>(int, int &)> &list)
{
    auto *tree = revisionTree();
    tree->clear();

    int err = ACTA_DB_OK;
    const QList<RevisionRow> revs = list(m_id, err);
    for (const RevisionRow &r : revs) {
        auto *item = new QTreeWidgetItem(tree);
        item->setText(0, QStringLiteral("Revision %1").arg(r.revision));
        item->setData(0, RoleRevisionId, r.id);
    }

    const int latest = revs.isEmpty() ? 0 : revs.last().revision;
    m_latestRevision = latest;

    if (!revs.isEmpty())
        // Select the latest (last, list is ascending) to show it.
        tree->setCurrentItem(tree->topLevelItem(revs.size() - 1));
    else if (err != ACTA_DB_OK)
        qWarning("revision lister failed: %s", acta_db_strerror(err));

    return latest;
}

void EntityDialog::onSaveClicked()
{
    if (!m_db)
        return;

    if (m_mode == Mode::New) {
        if (!validateFields())
            return;

        int newId = 0;
        const int rc = createEntity(&newId);
        if (rc != ACTA_DB_OK)
            return;

        // The row was persisted; the panel will reload on close.
        setSaved(true);

        // Keep the dialog open on the created row, now editable.
        m_id = newId;
        loadEntity(newId);
        setMode(Mode::Edit);
        setWindowTitle(editTitle());
        QMessageBox::information(this, m_title,
                                tr("%1 saved.").arg(m_title));
    } else if (m_mode == Mode::Edit && m_id != 0) {
        if (!validateFields())
            return;

        const int rc = updateEntity();
        if (rc != ACTA_DB_OK)
            return;

        // The update was persisted; the panel will reload on close.
        setSaved(true);

        // The update triggered a revision snapshot: reload and switch
        // back to the read-only view. The "Saved as revision N" feedback
        // explains why editing stopped (the dialog is now read-only).
        loadEntity(m_id);
        setMode(Mode::ReadOnly);
        setWindowTitle(showTitle());
        QMessageBox::information(
            this, m_title,
            tr("Saved as revision %1.").arg(m_latestRevision));
    }
}
