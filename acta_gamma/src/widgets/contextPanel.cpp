#include "contextPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QEvent>
#include <QKeyEvent>
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

    // "&" marks each button's accelerator (Alt+letter) (UR #39).
    // Icons + tooltips to match the other panels (P2 / UR #22).
    auto *btnStyle = style();
    auto *btnRow = new QHBoxLayout;
    newBtn = new QPushButton("&New");
    newBtn->setIcon(btnStyle->standardIcon(QStyle::SP_DialogYesButton));
    newBtn->setToolTip(tr("Create a new context"));
    showBtn = new QPushButton("S&how");
    showBtn->setIcon(btnStyle->standardIcon(QStyle::SP_DialogOpenButton));
    showBtn->setToolTip(
        tr("Show the details of the selected context (read-only)"));
    btnRow->addWidget(newBtn);
    btnRow->addWidget(showBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    // callback
    connect(newBtn, &QPushButton::clicked, this, &ContextPanel::onNewBtnClicked);
    connect(showBtn, &QPushButton::clicked, this, &ContextPanel::onShowBtnClicked);
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        applyTreeFilter(list, t, RoleContextContent);
    });

    // Keyboard accelerator (UR #39), handled in eventFilter() while the
    // list (or its viewport) has focus. Installed on both because either
    // widget can be the focus target after a click; the dialog is a
    // separate widget, so this never fires inside it.
    list->installEventFilter(this);
    list->viewport()->installEventFilter(this);

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

void ContextPanel::onReturnKeyPressed()
{
    // Enter opens the same read-only dialog as the Show button.
    onShowBtnClicked();
}

bool ContextPanel::eventFilter(QObject *obj, QEvent *event)
{
    // Enter opens the detail dialog. Consuming the key here, before
    // QTreeWidget's own handling, also prevents its built-in inline
    // editing on Return.
    if ((obj == list || obj == list->viewport())
            && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<const QKeyEvent *>(event);
        if (key->modifiers() == Qt::NoModifier
                && (key->key() == Qt::Key_Return
                        || key->key() == Qt::Key_Enter)) {
            onReturnKeyPressed();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
