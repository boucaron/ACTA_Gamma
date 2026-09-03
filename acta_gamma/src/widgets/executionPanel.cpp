#include "executionPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "executionDialog.h"
#include "executionCreateDialog.h"
#include "util.h"

namespace {
const int RoleExecutionId = Qt::UserRole;
const int RoleLogId = Qt::UserRole + 1;
} // namespace

ExecutionPanel::ExecutionPanel(db_t *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    // Panel section header (P2 / UR #21): the objectName targets the
    // #panelHeader rule of the app stylesheet.
    auto *titleLabel = new QLabel("Execution");
    titleLabel->setObjectName(QStringLiteral("panelHeader"));
    lay->addWidget(titleLabel);

    // Substring filter + status filter above the list (H4 / UR #38).
    auto *filterRow = new QHBoxLayout;
    filterEdit = new QLineEdit;
    filterEdit->setPlaceholderText(tr("Filter executions…"));
    filterRow->addWidget(filterEdit);
    statusFilter = new QComboBox;
    // "All" is the no-filter entry; the rest is the DB's status
    // vocabulary (acta_db/include/execution.h).
    statusFilter->addItems({"All",
                            ACTA_EXEC_STATUS_PENDING,
                            ACTA_EXEC_STATUS_RUNNING,
                            ACTA_EXEC_STATUS_COMPLETED,
                            ACTA_EXEC_STATUS_FAILED,
                            ACTA_EXEC_STATUS_CANCELLED});
    statusFilter->setToolTip("Show only executions with this status");
    filterRow->addWidget(statusFilter);
    lay->addLayout(filterRow);

    list = new QTreeWidget;
    // Skill / model / context names next to Date + Status (H5 / UR #23).
    list->setColumnCount(5);
    list->setHeaderLabels({"Date", "Status", "Skill", "Model",
                           "Context"});
    list->setSortingEnabled(true);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QTreeWidget::customContextMenuRequested, this,
            &ExecutionPanel::onListContextMenu);
    connect(list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showExecutionLogs(cur);
            });
    connect(list, &QTreeWidget::itemDoubleClicked, this,
            &ExecutionPanel::onExecutionDoubleClicked);
    lay->addWidget(list);

    // Centered placeholder over the blank list when the db is empty
    // (P5 / UR #31); shown/hidden in reload().
    emptyLabel = makeEmptyStateLabel(list, tr("No executions yet"));
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        applyFilters();
    });
    connect(statusFilter, &QComboBox::currentIndexChanged, this, [this](int) {
        applyFilters();
    });

    logList = new QTreeWidget;
    logList->setColumnCount(4);
    logList->setHeaderLabels({"Date", "Level", "Event", "Message"});
    logList->setRootIsDecorated(false);
    logList->setUniformRowHeights(true);
    lay->addWidget(logList);

    // Centered placeholder over the blank log list (P5 / UR #31);
    // shown/hidden in showExecutionLogs().
    emptyLogLabel = makeEmptyStateLabel(
        logList, tr("No log lines for this execution"));

    // The log list already shows all four log columns inline, so the
    // redundant "Show Log Details" button and log context menu were
    // dropped (P5 / UR #41).

    // New / Show button row (UR #22): "New" opens the create dialog
    // (a new row lands in "pending"; the runner that moves it through
    // start/complete/fail is phase 2). "Show" opens the execution
    // dialog for the selected execution row; the context menu and
    // double-click / Enter do the same (UR #33).
    auto *btnRow = new QHBoxLayout;
    newExecutionBtn = new QPushButton("&New");
    newExecutionBtn->setToolTip(tr("Create a new execution"));
    newExecutionBtn->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    btnRow->addWidget(newExecutionBtn);
    showDetailsBtn = new QPushButton("S&how");
    showDetailsBtn->setToolTip("Show the details of the selected execution");
    showDetailsBtn->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    btnRow->addWidget(showDetailsBtn);
    lay->addLayout(btnRow);

    // callbacks
    connect(newExecutionBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onNewBtnClicked);
    connect(showDetailsBtn, &QPushButton::clicked, this, [this] {
        onExecutionDoubleClicked(list->currentItem(), 0);
    });

    // Keyboard accelerator (UR #39), handled in eventFilter() while the
    // execution list (or its viewport) has focus. Installed on both
    // because either widget can be the focus target after a click; the
    // dialogs are separate widgets, so this never fires inside them.
    list->installEventFilter(this);
    list->viewport()->installEventFilter(this);
    // The log viewport filter only keeps emptyLogLabel centered on
    // resize; it handles no keys.
    logList->viewport()->installEventFilter(this);

    reload();
    // Initial state: no execution selected, so show the empty log
    // placeholder (showExecutionLogs only re-runs on selection change).
    showExecutionLogs(nullptr);
}

void ExecutionPanel::setDb(db_t *db)
{
    m_db = db;
    reload();
}

void ExecutionPanel::reload()
{
    // Preserve the current selection across the rebuild (UR #8).
    const auto *cur = list->currentItem();
    const int keepExecution = cur ? cur->data(0, RoleExecutionId).toInt() : 0;

    list->clear();
    if (!m_db) {
        emptyLabel->setVisible(true);
        return; // db open failed at startup; MainWindow surfaces the reason.
    }

    int n = 0;
    int err = ACTA_DB_OK;
    execution_t **executions =
        acta_db_execution_query(m_db, nullptr, 0, 0, &n, &err);
    if (!executions) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_query failed: %s",
                     acta_db_strerror(err));
        emptyLabel->setVisible(true);
        return;
    }

    for (int i = 0; i < n; ++i) {
        auto *item = new QTreeWidgetItem(list);
        // Locale-formatted date (UR #24); the display stays "yyyy-MM-dd
        // HH:mm" in every locale, so the column still sorts
        // chronologically as plain text. The exact ISO value (with
        // seconds) lives in the tooltip.
        const QString createdIso = executions[i]->created_at
            ? QString::fromUtf8(executions[i]->created_at)
            : QString();
        item->setText(0, displayDateTime(executions[i]->created_at));
        item->setToolTip(0, createdIso);

        const QString status = executions[i]->status
            ? QString::fromUtf8(executions[i]->status)
            : QString();
        item->setText(1, status);
        item->setForeground(1, statusColor(status)); // UR #25

        // Skill / model / context names (H5 / UR #23): the same revision
        // lookups that ExecutionDialog::editExecution() performs, reused
        // here so the panel shows what each row ran without opening the
        // dialog. Contexts have no name column; their type is the
        // identifying label (as in the context panel).
        skill_revision_t *srev = acta_db_skill_revision_get(
            m_db, executions[i]->skill_revision_id, &err);
        if (srev) {
            item->setText(
                2, QStringLiteral("%1 (rev %2)")
                       .arg(QString::fromUtf8(srev->name))
                       .arg(srev->revision));
            acta_db_skill_revision_free(srev);
        }
        model_revision_t *mrev = acta_db_model_revision_get(
            m_db, executions[i]->model_revision_id, &err);
        if (mrev) {
            item->setText(
                3, QStringLiteral("%1 (rev %2)")
                       .arg(QString::fromUtf8(mrev->name))
                       .arg(mrev->revision));
            acta_db_model_revision_free(mrev);
        }
        context_t *ctx =
            acta_db_context_get(m_db, executions[i]->context_id, &err);
        if (ctx) {
            item->setText(4, ctx->type ? QString::fromUtf8(ctx->type)
                                       : QString());
            acta_db_context_free(ctx);
        }

        item->setData(0, RoleExecutionId, executions[i]->id);
    }

    acta_db_execution_list_free(executions, n);

    // Re-select the previously current row (flat list: linear scan by
    // id); this also refills the log list via currentItemChanged.
    if (keepExecution != 0)
        for (int i = 0; i < list->topLevelItemCount(); ++i) {
            auto *item = list->topLevelItem(i);
            if (item->data(0, RoleExecutionId).toInt() == keepExecution) {
                list->setCurrentItem(item);
                break;
            }
        }

    // Re-apply the filters to the freshly built list (H4 / UR #38).
    applyFilters();

    // Empty-state placeholder: only when there are no rows at all
    // (P5 / UR #31).
    emptyLabel->setVisible(list->topLevelItemCount() == 0);
}

void ExecutionPanel::showExecutionLogs(QTreeWidgetItem *item)
{
    const int executionId = item ? item->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0 || !m_db) {
        logList->clear();
        emptyLogLabel->setVisible(true);
        return;
    }

    int n = 0;
    int err = ACTA_DB_OK;
    execution_log_t **lines =
        acta_db_execution_log_list_by_execution(m_db, executionId, nullptr,
                                                0, 0, &n, &err);
    logList->clear();
    emptyLogLabel->setVisible(true);
    if (!lines) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_log_list_by_execution(%d) failed: %s",
                     executionId, acta_db_strerror(err));
        return;
    }

    for (int i = 0; i < n; ++i) {
        const QString level =
            lines[i]->level ? QString::fromUtf8(lines[i]->level)
                            : QString();
        auto *logItem = new QTreeWidgetItem(logList, {
            displayDateTime(lines[i]->created_at), // UR #24
            level,
            lines[i]->event ? QString::fromUtf8(lines[i]->event) : QString(),
            lines[i]->message
                ? QString::fromUtf8(lines[i]->message)
                : QString(),
        });
        logItem->setToolTip(
            0, lines[i]->created_at ? QString::fromUtf8(lines[i]->created_at)
                                    : QString());
        logItem->setForeground(1, logLevelColor(level)); // UR #25
        logItem->setData(0, RoleLogId, lines[i]->id);
    }
    acta_db_execution_log_list_free(lines, n);

    // Empty-state placeholder for the log list (P5 / UR #31).
    emptyLogLabel->setVisible(logList->topLevelItemCount() == 0);
}

void ExecutionPanel::applyFilters()
{
    const QString needle = filterEdit ? filterEdit->text().trimmed() : QString();
    const QString status =
        statusFilter ? statusFilter->currentText() : QString();
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        auto *item = list->topLevelItem(i);
        bool ok = status.isEmpty() || status == QLatin1String("All")
                   || item->text(1) == status;
        if (ok && !needle.isEmpty()) {
            ok = false;
            for (int c = 0; c < item->columnCount(); ++c) {
                if (item->text(c).contains(needle, Qt::CaseInsensitive)) {
                    ok = true;
                    break;
                }
            }
        }
        item->setHidden(!ok);
    }
}

void ExecutionPanel::onNewBtnClicked()
{
    if (!m_db)
        return;

    ExecutionCreateDialog dlg(this);
    dlg.newExecution(m_db);
    dlg.exec();
    // Reload only when the dialog actually created an execution
    // (UR #8), then select the new row so the inline log list shows
    // its empty-state placeholder (currentItemChanged refills it).
    if (dlg.saved()) {
        reload();
        const int newId = dlg.createdId();
        // Linear scan by id (the same pattern reload() uses to
        // re-select the previously current row).
        for (int i = 0; i < list->topLevelItemCount(); ++i) {
            auto *item = list->topLevelItem(i);
            if (item->data(0, RoleExecutionId).toInt() == newId) {
                list->setCurrentItem(item);
                break;
            }
        }
    }
}

void ExecutionPanel::onExecutionDoubleClicked(QTreeWidgetItem *item, int)
{
    const int executionId =
        item ? item->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0 || !m_db)
        return;

    ExecutionDialog dlg(this);
    dlg.editExecution(m_db, executionId);
    // Execution rows are immutable: the dialog opens read-only and
    // changes nothing, so there is nothing to do (and no reload needed)
    // after it closes.
    dlg.exec();
}

void ExecutionPanel::onListContextMenu(const QPoint &pos)
{
    auto *item = list->itemAt(pos);
    if (!item)
        return;
    QMenu menu(this);
    auto *aShow = menu.addAction("Show");
    aShow->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    aShow->setToolTip("Show the details of this execution");
    connect(aShow, &QAction::triggered, this, [this, item] {
        onExecutionDoubleClicked(item, 0);
    });
    menu.exec(list->viewport()->mapToGlobal(pos));
}

void ExecutionPanel::onReturnKeyPressed()
{
    // Enter opens the same dialog as a double-click on the currently
    // selected execution row.
    onExecutionDoubleClicked(
        const_cast<QTreeWidgetItem *>(list->currentItem()), 0);
}

bool ExecutionPanel::eventFilter(QObject *obj, QEvent *event)
{
    // Keep the empty-state labels centered when a viewport resizes
    // (P5 / UR #31); the event passes through.
    if ((obj == list->viewport() || obj == logList->viewport())
            && event->type() == QEvent::Resize) {
        if (obj == list->viewport())
            placeEmptyStateLabel(emptyLabel, list);
        else
            placeEmptyStateLabel(emptyLogLabel, logList);
        return QWidget::eventFilter(obj, event);
    }
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
