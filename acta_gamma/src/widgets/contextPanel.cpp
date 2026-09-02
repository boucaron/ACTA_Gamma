#include "contextPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextEdit>
#include <QPushButton>
#include <QStyle>

#include "contextDialog.h"
#include "util.h"

namespace {
const int RoleContextId = Qt::UserRole;
// Row payload for the search filter: the content is not a visible
// column (the list shows Type + Date only) but is the main identifying
// data, so it is stored per row and searched by applyTreeFilter
// (H4 / UR #38).
const int RoleContextContent = Qt::UserRole + 1;
} // namespace

ContextPanel::ContextPanel(db_t *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    // Panel section header (P2 / UR #21): the objectName targets the
    // #panelHeader rule of the app stylesheet.
    auto *titleLabel = new QLabel("Context");
    titleLabel->setObjectName(QStringLiteral("panelHeader"));
    lay->addWidget(titleLabel);

    // Case-insensitive substring filter above the list (H4 / UR #38).
    filterEdit = new QLineEdit;
    filterEdit->setPlaceholderText(tr("Filter contexts…"));
    lay->addWidget(filterEdit);

    list = new QTreeWidget;
    list->setColumnCount(2);
    list->setHeaderLabels({"Type", "Date"});
    list->setSortingEnabled(true);
    connect(list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showContext(cur);
            });
    lay->addWidget(list);

    // Centered placeholder over the blank list when the db is empty
    // (P5 / UR #31); shown/hidden in reload().
    emptyLabel = makeEmptyStateLabel(list, tr("No contexts yet — click New"));

    editor = new QTextEdit;
    editor->setReadOnly(true); // contexts are immutable; this is display-only
    editor->setPlaceholderText("Immutable input JSON...");
    lay->addWidget(editor);

    // "&" marks the button's accelerator (Alt+letter) (UR #39).
    // Icon + tooltip to match the other panels (P2 / UR #22).
    // Single-clicking a row already fills the inline editor below, so
    // the redundant "Show" button, context menu and Enter accelerator
    // were dropped (P5 / UR #41).
    auto *btnStyle = style();
    auto *btnRow = new QHBoxLayout;
    newBtn = new QPushButton("&New");
    newBtn->setIcon(btnStyle->standardIcon(QStyle::SP_DialogYesButton));
    newBtn->setToolTip(tr("Create a new context"));
    btnRow->addWidget(newBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    // callback
    connect(newBtn, &QPushButton::clicked, this, &ContextPanel::onNewBtnClicked);
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        applyTreeFilter(list, t, RoleContextContent);
    });

    // Keeps emptyLabel centered as the viewport resizes (P5 / UR #31).
    list->viewport()->installEventFilter(this);

    reload();
}

bool ContextPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == list->viewport() && event->type() == QEvent::Resize) {
        placeEmptyStateLabel(emptyLabel, list);
        return QWidget::eventFilter(obj, event);
    }
    return QWidget::eventFilter(obj, event);
}

void ContextPanel::setDb(db_t *db)
{
    m_db = db;
    reload();
}

void ContextPanel::reload()
{
    list->clear();
    if (!m_db) {
        emptyLabel->setVisible(true);
        return; // db open failed at startup; MainWindow surfaces the reason.
    }

    int n = 0;
    int err = ACTA_DB_OK;
    context_t **contexts = acta_db_context_query(m_db, nullptr, 0, 0, &n, &err);
    if (!contexts) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_query failed: %s",
                     acta_db_strerror(err));
        emptyLabel->setVisible(true);
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = new QTreeWidgetItem(list);
        item->setText(0, contexts[i]->type && contexts[i]->type[0]
                             ? QString::fromUtf8(contexts[i]->type)
                             : QStringLiteral("context"));
        // Locale-formatted date (UR #24); the display stays
        // "yyyy-MM-dd HH:mm" in every locale, so the column still
        // sorts chronologically as plain text. The exact ISO value
        // (with seconds) lives in the tooltip.
        const QString createdIso = contexts[i]->created_at
            ? QString::fromUtf8(contexts[i]->created_at)
            : QString();
        item->setText(1, displayDateTime(contexts[i]->created_at));
        item->setToolTip(1, createdIso);
        item->setData(0, RoleContextId, contexts[i]->id);
        item->setData(0, RoleContextContent,
                      contexts[i]->content
                          ? QString::fromUtf8(contexts[i]->content)
                          : QString());
    }

    acta_db_context_list_free(contexts, n);

    // Re-apply the filter to the freshly built list (H4 / UR #38).
    applyTreeFilter(list, filterEdit ? filterEdit->text() : QString(),
                    RoleContextContent);

    // Empty-state placeholder: only when there are no rows at all
    // (P5 / UR #31).
    emptyLabel->setVisible(list->topLevelItemCount() == 0);
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
