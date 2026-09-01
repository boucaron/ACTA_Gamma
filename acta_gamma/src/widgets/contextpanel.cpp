#include "contextPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QTextEdit>
#include <QPushButton>

#include "contextDialog.h"

namespace {
const int RoleContextId = Qt::UserRole;
} // namespace

ContextPanel::ContextPanel(db_t *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Context"));

    list = new QTreeWidget;
    list->setColumnCount(2);
    list->setHeaderLabels({"Type", "Date"});
    list->setSortingEnabled(true);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QTreeWidget::customContextMenuRequested,
            this, &ContextPanel::onListContextMenu);
    connect(list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showContext(cur);
            });
    lay->addWidget(list);

    editor = new QTextEdit;
    editor->setReadOnly(true); // contexts are immutable; this is display-only
    editor->setPlaceholderText("Immutable input JSON...");
    lay->addWidget(editor);

    auto *btnRow = new QHBoxLayout;
    newBtn = new QPushButton("New");
    showBtn = new QPushButton("Show");
    btnRow->addWidget(newBtn);
    btnRow->addWidget(showBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    // callback
    connect(newBtn, &QPushButton::clicked, this, &ContextPanel::onNewBtnClicked);
    connect(showBtn, &QPushButton::clicked, this, &ContextPanel::onShowBtnClicked);

    reload();
}

void ContextPanel::reload()
{
    list->clear();
    if (!m_db)
        return; // db open failed at startup; MainWindow surfaces the reason.

    int n = 0;
    int err = ACTA_DB_OK;
    context_t **contexts = acta_db_context_query(m_db, nullptr, 0, 0, &n, &err);
    if (!contexts) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_query failed: %s",
                     acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = new QTreeWidgetItem(list);
        item->setText(0, contexts[i]->type && contexts[i]->type[0]
                             ? QString::fromUtf8(contexts[i]->type)
                             : QStringLiteral("context"));
        // ISO "yyyy-MM-dd HH:mm:ss" sorts correctly as plain text.
        item->setText(1, contexts[i]->created_at
                             ? QString::fromUtf8(contexts[i]->created_at)
                             : QString());
        item->setData(0, RoleContextId, contexts[i]->id);
    }

    acta_db_context_list_free(contexts, n);
}

void ContextPanel::showContext(QTreeWidgetItem *item)
{
    const int contextId = item ? item->data(0, RoleContextId).toInt() : 0;
    if (contextId == 0 || !m_db) {
        editor->clear();
        return;
    }

    int err = ACTA_DB_OK;
    context_t *c = acta_db_context_get(m_db, contextId, &err);
    if (!c) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_get(%d) failed: %s", contextId,
                     acta_db_strerror(err));
        editor->clear();
        return;
    }
    editor->setPlainText(c->content ? c->content : "");
    acta_db_context_free(c);
}

void ContextPanel::onListContextMenu(const QPoint &pos)
{
    auto *item = list->itemAt(pos);
    if (!item)
        return;
    const int contextId = item->data(0, RoleContextId).toInt();
    QMenu menu(this);
    // Contexts are immutable; the dialog opens read-only, so the
    // entry is labelled "Show", not "Edit".
    menu.addAction(tr("Show"), this,
                   [this, contextId] { editContext(contextId); });
    menu.exec(list->viewport()->mapToGlobal(pos));
}

void ContextPanel::editContext(int contextId)
{
    if (!m_db)
        return;

    ContextDialog dlg(this);
    dlg.editContext(m_db, contextId);
    // Contexts are immutable: the dialog opens read-only and changes
    // nothing, so there is nothing to do (and no reload needed) after
    // it closes.
    dlg.exec();
}

void ContextPanel::onNewBtnClicked()
{
    if (!m_db)
        return;

    ContextDialog dlg(this);
    dlg.newContext(m_db);
    dlg.exec();
    // The save happened inside the dialog; refresh the list either way.
    reload();
}

void ContextPanel::onShowBtnClicked()
{
    const QTreeWidgetItem *cur = list->currentItem();
    const int contextId = cur ? cur->data(0, RoleContextId).toInt() : 0;
    if (contextId == 0 || !m_db)
        return;

    ContextDialog dlg(this);
    dlg.editContext(m_db, contextId);
    dlg.exec();
}
