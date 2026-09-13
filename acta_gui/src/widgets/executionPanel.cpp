#include "executionPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QKeySequence>
#include <QMessageBox>
#include <QThread>
#include <QPushButton>
#include <QStyle>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "runner.h" // EXIT_CANCELED
#include "runnerWorker.h"
#include "executionDialog.h"
#include "executionCreateDialog.h"
#include "executionLogDialog.h"
#include "util.h"

namespace {
const int RoleExecutionId = Qt::UserRole;
// Log line id carried by the log table model (first column), used by
// the Show button / context menu to open the log dialog.
const int RoleLogId = Qt::UserRole + 1;
// Soft-deleted marker on the execution list rows (true when the row's
// deleted_at is set); only present when "Show trash" is checked.
const int RoleExecutionDeleted = Qt::UserRole + 2;

// Backend timeout for the in-process run, matching the CLI's default
// (acta_runner help: --timeout, default 300).
const int kRunnerTimeoutSec = 300;
} // namespace

ExecutionPanel::ExecutionPanel(db_t *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    auto *lay = new QVBoxLayout(this);
    // Panel section header (P2 / UR #21): the objectName targets the
    // #panelHeader rule of the app stylesheet.
    auto *titleLabel = new QLabel(tr("Execution"));
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
    statusFilter->addItems({tr("All"),
                            ACTA_EXEC_STATUS_PENDING,
                            ACTA_EXEC_STATUS_RUNNING,
                            ACTA_EXEC_STATUS_COMPLETED,
                            ACTA_EXEC_STATUS_FAILED,
                            ACTA_EXEC_STATUS_CANCELLED});
    statusFilter->setToolTip(tr("Show only executions with this status"));
    filterRow->addWidget(statusFilter);
    lay->addLayout(filterRow);

    // "Show trash" (mirrors the skill/model panels): soft-deleted
    // executions stay in the database and can be restored, so the
    // trash label is the accurate (and shorter) one. Unchecked by
    // default: deleted rows are hidden.
    showDeletedCheck = new QCheckBox(tr("Show trash"));
    showDeletedCheck->setToolTip(
        tr("Show soft-deleted executions (the trash); they stay in the "
           "database and can be restored."));
    lay->addWidget(showDeletedCheck);

    list = new QTreeWidget;
    // Skill / model / context names next to Date + Status (H5 / UR #23).
    list->setColumnCount(5);
    list->setHeaderLabels({tr("Date"), tr("Status"), tr("Skill"), tr("Model"),
                           tr("Context")});
    list->setSortingEnabled(true);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QTreeWidget::customContextMenuRequested, this,
            &ExecutionPanel::onListContextMenu);
    connect(list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                showExecutionLogs(cur);
                emitItemChanged();
                updateRunBtnState();
                updateActionBtnStates();
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

    // Flat log table (UR #23): a plain QTableView — the same control as
    // the execution dialog's log table. The 4-column model is rebuilt per
    // selection in showExecutionLogs(). (A QTableWidget was not used:
    // its setModel is private, and setUniformRowHeights, set on the old
    // QTreeWidget, doesn't exist on QTableView in Qt 6.11; each row now
    // auto-sizes to its own content, so one tall message only grows its
    // own row.)
    logList = new QTableView;
    lay->addWidget(logList);
    // Context menu on the log list: "Show" opens the read-only
    // execution log dialog for the selected line (same pairing as the
    // execution list's Show context menu, UR #33).
    logList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(logList, &QTableView::customContextMenuRequested, this,
            &ExecutionPanel::onLogListContextMenu);
    // Selection drives the Show button's enabled state; the
    // selectionChanged connection is (re)established in setLogModel()
    // after every model swap, because QItemView replaces the
    // selection model when the table's model changes.

    // Centered placeholder over the blank log list (P5 / UR #31);
    // shown/hidden in showExecutionLogs().
    emptyLogLabel = makeEmptyStateLabel(
        logList, tr("No log lines for this execution"));

    // Icon-only toolbar row (P2 / UR #22), matching the other panels:
    // tooltips carry the meaning, accelerators are Alt+letter (UR #39).
    // "New" opens the create dialog (a new row lands in "pending"; the
    // runner that moves it through start/complete/fail is the in-process
    // worker (M1 / UR #45) — the in-app "Run" button (Plan D) starts it).
    // "Show" opens the execution dialog for the selected execution row;
    // the context menu and double-click / Enter do the same (UR #33).
    m_deletedIcon = list->style()->standardIcon(QStyle::SP_TrashIcon);

    auto *btnRow = new QHBoxLayout;
    newExecutionBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_DialogYesButton),
        tr("Create a new execution"),
        QKeySequence(Qt::ALT | Qt::Key_N));
    btnRow->addWidget(newExecutionBtn);
    showDetailsBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("Show the details of the selected execution"),
        QKeySequence(Qt::ALT | Qt::Key_H));
    btnRow->addWidget(showDetailsBtn);
    // "Run" (R1 / Plan D): runs the selected execution through the
    // in-process runner worker (M1 / UR #45) and polls the DB for
    // live status + phase log rows (UR #18 remainder, UR #44).
    runBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_MediaPlay),
        tr("Run the selected execution (a failed execution is reset to "
           "pending first)"),
        QKeySequence(Qt::ALT | Qt::Key_R));
    btnRow->addWidget(runBtn);
    // Soft-delete lifecycle for the selected row (mirrors the model/
    // skill panels): Delete is only enabled for live rows that are not
    // running (deleting a running row is forbidden at the C layer);
    // Restore only for deleted rows.
    deleteBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_TrashIcon),
        tr("Soft-delete the selected execution (it stays in the "
           "database and can be restored)"),
        QKeySequence(Qt::ALT | Qt::Key_D));
    btnRow->addWidget(deleteBtn);
    restoreBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_ArrowBack),
        tr("Restore the selected soft-deleted execution"),
        QKeySequence(Qt::ALT | Qt::Key_T));
    btnRow->addWidget(restoreBtn);
    // "Show Log": opens the execution log dialog for the selected log
    // line of the inline log list; the log list's context menu does
    // the same (UR #33, mirroring the Show/Show-Log button pair).
    showLogBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("Show the details of the selected log line"),
        QKeySequence(Qt::ALT | Qt::Key_L));
    btnRow->addWidget(showLogBtn);
    lay->addLayout(btnRow);

    // callbacks
    connect(newExecutionBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onNewBtnClicked);
    connect(showDetailsBtn, &QPushButton::clicked, this, [this] {
        onExecutionDoubleClicked(list->currentItem(), 0);
    });
    connect(runBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onRunBtnClicked);
    connect(deleteBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onExecuteDeleteClicked);
    connect(restoreBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onExecuteRestoreClicked);
    connect(showDeletedCheck, &QCheckBox::toggled, this, [this](bool) {
        reload();
    });
    connect(showLogBtn, &QPushButton::clicked, this, [this] {
        showLogDetails(selectedLogId());
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
    setDb(db, QString());
}

ExecutionPanel::~ExecutionPanel()
{
    stopRunner();
}

void ExecutionPanel::setDb(db_t *db, const QString &dbPath)
{
    stopRunner();
    m_db = db;
    m_dbPath = dbPath;
    reload();
}

void ExecutionPanel::stopRunner()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    if (m_runnerThread) {
        // The database switched underneath the worker (or the app is
        // shutting down). The worker thread cannot be cancelled
        // mid-HTTP (the runner's pipeline has no cancellation hook),
        // so let it run to completion — bounded by the backend
        // timeout — so the execution row never stays stuck in
        // "running". Its own DB connection closes with the worker, so
        // the stale database only receives that execution's final
        // complete/fail.
        //
        // The worker is parent-less (it lives on the worker thread via
        // moveToThread, so nothing can own it there): post its deletion
        // to the worker's event queue BEFORE quitting. The deferred
        // delete is then processed just before the quit event ends the
        // loop, so the worker self-destructs on its own thread and
        // wait() cannot return with a dangling worker.
        m_runnerWorker->deleteLater();
        m_runnerThread->quit();
        m_runnerThread->wait();
        delete m_runnerThread;
        m_runnerThread = nullptr;
        m_runnerWorker = nullptr;
    }
    m_runningExecutionId = 0;
}

void ExecutionPanel::updateRunBtnState()
{
    if (m_runnerThread) {
        // While the pipeline is in flight the button is Cancel: enabled
        // unconditionally, with the cancel icon and tooltip.
        runBtn->setEnabled(true);
        runBtn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
        runBtn->setToolTip(tr("Cancel the running execution"));
        return;
    }
    runBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    runBtn->setToolTip(
        tr("Run the selected execution (a failed execution is reset to "
           "pending first)"));
    bool canRun = m_db != nullptr;
    if (canRun) {
        const auto *cur = list->currentItem();
        // Deleted rows are inert: they must be restored before they can
        // be run (reset() refuses them at the DB layer).
        canRun = cur != nullptr
            && !cur->data(0, RoleExecutionDeleted).toBool()
            && (cur->text(1) == QLatin1String(ACTA_EXEC_STATUS_PENDING)
                || cur->text(1) == QLatin1String(ACTA_EXEC_STATUS_FAILED));
    }
    runBtn->setEnabled(canRun);
}

void ExecutionPanel::updateActionBtnStates()
{
    const auto *cur = list->currentItem();
    const bool hasRow =
        cur != nullptr && cur->data(0, RoleExecutionId).toInt() != 0;
    const bool isDeleted =
        hasRow && cur->data(0, RoleExecutionDeleted).toBool();
    // While a run is in flight the status cell shows "running…" (the
    // progress indicator), so match by prefix, not exact equality.
    const bool isRunning =
        hasRow
        && cur->text(1).startsWith(QLatin1String(ACTA_EXEC_STATUS_RUNNING));
    // Delete is forbidden from the running state (the runner owns the
    // row) and on already-deleted rows — the same rule acta_db
    // enforces (INVALID / NOT_FOUND respectively).
    deleteBtn->setEnabled(hasRow && !isDeleted && !isRunning);
    restoreBtn->setEnabled(hasRow && isDeleted);
}

QTreeWidgetItem *ExecutionPanel::findRow(int executionId) const
{
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        auto *item = list->topLevelItem(i);
        if (item->data(0, RoleExecutionId).toInt() == executionId)
            return item;
    }
    return nullptr;
}

void ExecutionPanel::onRunBtnClicked()
{
    // While a run is in flight the button is the Cancel button: set the
    // runner's cooperative cancel flag. The worker exits through the
    // cancel path (row pending|running -> cancelled) and the UI
    // re-syncs on the worker-finished signal.
    if (m_runnerThread) {
        m_runnerWorker->requestCancel();
        return;
    }
    if (!m_db)
        return;
    const auto *cur = list->currentItem();
    const int executionId = cur ? cur->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0)
        return;

    int err = ACTA_DB_OK;
    execution_t *exec = acta_db_execution_get(m_db, executionId, &err);
    if (!exec) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_get(%d) failed: %s",
                     executionId, acta_db_strerror(err));
        return;
    }
    // Deleted rows are inert (restore before running): the button is
    // disabled for them, but the context menu / shortcut can fire on a
    // stale selection, so guard here as well. The runner atomically
    // refuses non-pending rows via start() anyway; the status check is
    // a UI convenience for the same reason.
    const bool deleted = exec->deleted_at != nullptr;
    const QString status =
        exec->status ? QString::fromUtf8(exec->status) : QString();
    acta_db_execution_free(exec);
    if (deleted
            || status != QLatin1String(ACTA_EXEC_STATUS_PENDING)
            && status != QLatin1String(ACTA_EXEC_STATUS_FAILED))
        return;

    // A failed row is retried: reset failed -> pending first, so the
    // runner's start() claim succeeds. The previous attempt's log lines
    // stay in the audit trail. On failure, do not start the worker.
    if (status == QLatin1String(ACTA_EXEC_STATUS_FAILED)) {
        int rc = acta_db_execution_reset(m_db, executionId);
        if (rc != ACTA_DB_OK) {
            qWarning("acta_db_execution_reset(%d) failed: %s",
                     executionId, acta_db_strerror(rc));
            return;
        }
        // Reflect the reset in the list immediately (the runner's polling
        // keeps it in sync from here on).
        QTreeWidgetItem *item = findRow(executionId);
        if (item) {
            const QString pending =
                QLatin1String(ACTA_EXEC_STATUS_PENDING);
            item->setText(1, pending);
            item->setForeground(1, statusColor(pending));
            item->setToolTip(1, statusMeaning(pending));
        }
    }

    // Start the in-process runner worker (M1 / UR #45): it runs
    // run_execution() on its own thread with its own DB connection and
    // emits finished() with the exit code and the failure message read
    // from the execution's log (no stderr parsing).
    m_runningExecutionId = executionId;
    m_runnerWorker =
        new RunnerWorker(executionId, m_dbPath, kRunnerTimeoutSec);
    m_runnerThread = new QThread(this);
    // moveToThread, not setParent: a QThread object *lives* on the
    // thread that created it (the GUI thread) and only *runs* on the
    // new one; reparenting to it would leave the worker on the GUI
    // thread and dispatch runInThread() — and its blocking HTTP
    // pipeline — into the GUI event loop, freezing the UI for the
    // whole run. Ownership is manual instead: stopRunner() posts
    // deleteLater() to the worker's queue before quit() + wait().
    m_runnerWorker->moveToThread(m_runnerThread);
    connect(m_runnerWorker, &RunnerWorker::finished, this,
            &ExecutionPanel::onWorkerFinished);
    connect(m_runnerThread, &QThread::started, m_runnerWorker,
            &RunnerWorker::runInThread);
    m_runnerThread->start();
    // Poll the DB while the runner is active (Plan D: the database is
    // the message bus; WAL supports the concurrent reader here).
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1500);
    connect(m_pollTimer, &QTimer::timeout, this, &ExecutionPanel::onPollTick);
    m_pollTimer->start();
    refreshRunningRow();
    updateRunBtnState();
}

void ExecutionPanel::onWorkerFinished(int exitCode, const QString &message)
{
    // The database may have switched underneath the worker: it ran on
    // its own connection to the old file, so sync the UI only when the
    // worker's file is still the current one.
    const bool sameDb = m_runnerWorker != nullptr
        && m_runnerWorker->dbPath() == m_dbPath;
    if (sameDb) {
        // Final targeted refresh; the row is already completed/failed
        // in the DB, the refresh only syncs the display.
        refreshRunningRow();
        refreshRunningLogs();
    }
    stopRunner();
    updateActionBtnStates();

    if (sameDb && exitCode != 0 && exitCode != EXIT_CANCELED) {
        // The execution row already carries the error; this is purely
        // informational (P4 error-surfacing style). A user-initiated
        // cancel is not an error: the row just shows "cancelled".
        QMessageBox::warning(
            this,
            tr("Execution failed"),
            message.isEmpty()
                ? tr("The runner exited with code %1").arg(exitCode)
                : message);
    }

    updateRunBtnState();
}

void ExecutionPanel::onPollTick()
{
    if (!m_db || m_runningExecutionId == 0)
        return;
    refreshRunningRow();
    refreshRunningLogs();
}

void ExecutionPanel::refreshRunningRow()
{
    if (m_runningExecutionId == 0 || !m_db)
        return;
    int err = ACTA_DB_OK;
    execution_t *exec =
        acta_db_execution_get(m_db, m_runningExecutionId, &err);
    if (!exec) {
        // Transient DB error (e.g. SQLITE_BUSY) or the row vanished:
        // skip this tick, retry on the next one.
        if (err != ACTA_DB_OK)
            qWarning("acta_db_execution_get(%d) failed: %s",
                     m_runningExecutionId, acta_db_strerror(err));
        return;
    }
    const QString status =
        exec->status ? QString::fromUtf8(exec->status) : QString();
    acta_db_execution_free(exec);

    QTreeWidgetItem *item = findRow(m_runningExecutionId);
    if (!item)
        return;
    // Progress indicator while running (UR #44): "running…"; the color
    // and tooltip keep describing the plain status.
    const QString shown =
        status == QLatin1String(ACTA_EXEC_STATUS_RUNNING)
            ? status + QStringLiteral("\u2026")
            : status;
    item->setText(1, shown);
    item->setForeground(1, statusColor(status));
    item->setToolTip(1, statusMeaning(status));
    updateRunBtnState();
}

void ExecutionPanel::refreshRunningLogs()
{
    if (m_runningExecutionId == 0 || !m_db)
        return;
    QTreeWidgetItem *item = findRow(m_runningExecutionId);
    // Only refresh the log list while the running row is selected;
    // switching rows refills the log via the selection handler.
    if (!item || list->currentItem() != item)
        return;
    // Auto-scroll (UR #44): follow the newest line only when the user
    // was already at the last row (or there were no rows yet).
    const int prevCount =
        logList->model() ? logList->model()->rowCount() : 0;
    const bool atLast =
        prevCount == 0 || logList->currentIndex().row() == prevCount - 1;
    showExecutionLogs(item);
    const int newCount =
        logList->model() ? logList->model()->rowCount() : 0;
    if (newCount > prevCount && atLast) {
        logList->selectRow(newCount - 1);
        logList->scrollTo(logList->model()->index(newCount - 1, 0));
    }
}

void ExecutionPanel::emitItemChanged()
{
    const auto *cur = list->currentItem();
    const int id = cur ? cur->data(0, RoleExecutionId).toInt() : 0;
    if (id == m_lastEmittedId)
        return;
    m_lastEmittedId = id;
    Q_EMIT itemChanged(id);
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
    // Live rows by default; with "Show trash" the query also returns
    // soft-deleted rows so they can be restored.
    execution_query_t q = ACTA_EXEC_QUERY_ANY;
    q.include_deleted =
        showDeletedCheck != nullptr && showDeletedCheck->isChecked();
    execution_t **executions =
        acta_db_execution_query(m_db, &q, 0, 0, &n, &err);
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
        item->setToolTip(1, statusMeaning(status)); // UR #37

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

        // Soft-deleted row (only visible with "Show trash"): trash icon,
        // gray date and a deleted tooltip, mirroring the skill/model
        // panel rows. The marker drives the Delete / Restore / Run
        // button states.
        if (executions[i]->deleted_at != nullptr) {
            item->setData(0, RoleExecutionDeleted, true);
            item->setIcon(0, m_deletedIcon);
            item->setForeground(0, QColor(Qt::gray));
            item->setToolTip(
                0, tr("Deleted %1").arg(createdIso));
        }
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

    // Notify about the (possibly changed) selection after the rebuild
    // (UR #19); the last-emitted id guard suppresses duplicates when
    // the selection was preserved across the reload.
    emitItemChanged();

    // Re-evaluate the Run button (R1) against the rebuilt list.
    updateRunBtnState();
    updateActionBtnStates();
}

void ExecutionPanel::setLogModel(QStandardItemModel *model)
{
    logList->setModel(model);
    // QItemView creates a new QItemSelectionModel per model, so the
    // selectionChanged connection must be re-established after every
    // swap; the old model (and its selection model) is destroyed,
    // so this never accumulates dead connections. The context object
    // (logList) keeps the lambda's connection scoped to the view.
    connect(logList->selectionModel(),
            &QItemSelectionModel::selectionChanged, logList,
            [this](const QItemSelection &, const QItemSelection &) {
                updateLogBtnState();
            });
    // A model swap invalidates the previous selection; sync the Show
    // button's enabled state (selectionChanged also fires, this keeps
    // the state consistent when no selection remains).
    updateLogBtnState();
}

void ExecutionPanel::showExecutionLogs(QTreeWidgetItem *item)
{
    const int executionId = item ? item->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0 || !m_db) {
        // QTableView has no clear(); a fresh empty model (with the
        // column headers, like the dialog builds) is the equivalent.
        auto *emptyModel = new QStandardItemModel(0, 4);
        emptyModel->setHorizontalHeaderLabels(
            {tr("Date"), tr("Level"),
             tr("Event"), tr("Message")});
        setLogModel(emptyModel);
        emptyLogLabel->setVisible(true);
        return;
    }

    int n = 0;
    int err = ACTA_DB_OK;
    execution_log_t **lines =
        acta_db_execution_log_list_by_execution(m_db, executionId, nullptr,
                                                0, 0, &n, &err);
    // One model per load, exactly as ExecutionDialog::editExecution()
    // builds its log table (same columns, same tooltips, same level
    // colors), so panel and dialog stay visually in lockstep.
    auto *model = new QStandardItemModel(0, 4);
    model->setHorizontalHeaderLabels(
        {tr("Date"), tr("Level"),
         tr("Event"), tr("Message")});
    if (lines) {
        for (int i = 0; i < n; ++i) {
            const QString level =
                lines[i]->level ? QString::fromUtf8(lines[i]->level)
                                : QString();
            auto *dateItem = new QStandardItem(
                displayDateTime(lines[i]->created_at)); // UR #24
            dateItem->setToolTip(
                lines[i]->created_at
                    ? QString::fromUtf8(lines[i]->created_at)
                    : QString());
            // Show target (Qt 6.11: QDataViewModelItem::setData(value, role)).
            dateItem->setData(QVariant(lines[i]->id), RoleLogId);
            auto *levelItem = new QStandardItem(level);
            levelItem->setForeground(logLevelColor(level)); // UR #25
            model->appendRow({
                dateItem,
                levelItem,
                new QStandardItem(lines[i]->event
                                    ? QString::fromUtf8(lines[i]->event)
                                    : QString()),
                new QStandardItem(lines[i]->message
                                    ? QString::fromUtf8(lines[i]->message)
                                    : QString()),
            });
        }
        acta_db_execution_log_list_free(lines, n);
    } else if (err != ACTA_DB_OK) {
        qWarning("acta_db_execution_log_list_by_execution(%d) failed: %s",
                 executionId, acta_db_strerror(err));
    }
    setLogModel(model);

    // Empty-state placeholder for the log list (P5 / UR #31).
    emptyLogLabel->setVisible(model->rowCount() == 0);
}

void ExecutionPanel::applyFilters()
{
    const QString needle = filterEdit ? filterEdit->text().trimmed() : QString();
    const QString status =
        statusFilter ? statusFilter->currentText() : QString();
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        auto *item = list->topLevelItem(i);
        bool ok = status.isEmpty() || status == tr("All")
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

void ExecutionPanel::updateLogBtnState()
{
    const QModelIndex idx = logList->currentIndex();
    showLogBtn->setEnabled(idx.isValid() && logList->model() != nullptr);
}

int ExecutionPanel::selectedLogId() const
{
    const QModelIndex idx = logList->currentIndex();
    if (!idx.isValid() || !logList->model())
        return 0;
    // The log id is stored in column 0 (QDataViewModelItem::data only
    // serves its payload for column 0), so read the row's sibling there
    // no matter which column the cursor is on.
    const QModelIndex idIdx = logList->model()->index(idx.row(), 0);
    return logList->model()->data(idIdx, RoleLogId).toInt();
}

void ExecutionPanel::showLogDetails(int logId)
{
    if (!m_db || logId == 0)
        return;

    ExecutionLogDialog dlg(this);
    dlg.showLog(m_db, logId);
    dlg.exec();
}

void ExecutionPanel::onLogListContextMenu(const QPoint &pos)
{
    const QModelIndex idx = logList->indexAt(pos);
    // A right-click on a row selects it; a right-click in the empty
    // area keeps the current selection — the menu is still shown, with
    // the entry falling back to the button state (L1).
    if (idx.isValid()) {
        logList->setCurrentIndex(idx);
        updateLogBtnState();
    }
    QMenu menu(this);
    auto *aShow = menu.addAction(tr("Show"));
    aShow->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    aShow->setToolTip(tr("Show the details of this log line"));
    aShow->setEnabled(showLogBtn->isEnabled());
    connect(aShow, &QAction::triggered, this, [this] {
        showLogDetails(selectedLogId());
    });
    menu.exec(logList->viewport()->mapToGlobal(pos));
}

void ExecutionPanel::onListContextMenu(const QPoint &pos)
{
    auto *item = list->itemAt(pos);
    // A right-click on a row selects it; a right-click in the empty
    // area keeps the current selection — the menu is still shown, with
    // the row-scoped actions falling back to the button states (L1).
    if (item)
        list->setCurrentItem(item);
    QMenu menu(this);
    // Mirrors the other panels' context menus (UR #22): "New…" first,
    // then the row action.
    auto *aNew = menu.addAction(tr("New…"));
    aNew->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    aNew->setToolTip(tr("Create a new execution"));
    connect(aNew, &QAction::triggered, this, [this] { onNewBtnClicked(); });
    // The menu entry mirrors the button: Run, or Cancel while a run is
    // in flight (it triggers the same onRunBtnClicked branch).
    if (m_runnerThread) {
        auto *aCancel = menu.addAction(tr("Cancel"));
        aCancel->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
        aCancel->setToolTip(tr("Cancel the running execution"));
        aCancel->setEnabled(runBtn->isEnabled());
        connect(aCancel, &QAction::triggered, this, [this] {
            onRunBtnClicked();
        });
    } else {
        auto *aRun = menu.addAction(tr("Run"));
        aRun->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        aRun->setToolTip(tr("Run the selected execution (a failed execution "
                             "is reset to pending first)"));
        aRun->setEnabled(runBtn->isEnabled());
        connect(aRun, &QAction::triggered, this, [this] {
            onRunBtnClicked();
        });
    }
    // Soft-delete lifecycle entries, enabled exactly like the toolbar
    // buttons (Delete: live, non-running row; Restore: deleted row).
    auto *aDelete = menu.addAction(tr("Delete"));
    aDelete->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    aDelete->setToolTip(tr("Soft-delete the selected execution (it stays "
                            "in the database and can be restored)"));
    aDelete->setEnabled(deleteBtn->isEnabled());
    connect(aDelete, &QAction::triggered, this, [this] {
        onExecuteDeleteClicked();
    });
    auto *aRestore = menu.addAction(tr("Restore"));
    aRestore->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    aRestore->setToolTip(tr("Restore the selected soft-deleted execution"));
    aRestore->setEnabled(restoreBtn->isEnabled());
    connect(aRestore, &QAction::triggered, this, [this] {
        onExecuteRestoreClicked();
    });
    auto *aShow = menu.addAction(tr("Show"));
    aShow->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    aShow->setToolTip(tr("Show the details of this execution"));
    aShow->setEnabled(showDetailsBtn->isEnabled());
    // Operates on the currently selected row, like the Show button.
    connect(aShow, &QAction::triggered, this, [this] {
        onExecutionDoubleClicked(list->currentItem(), 0);
    });
    menu.exec(list->viewport()->mapToGlobal(pos));
}

void ExecutionPanel::onExecuteDeleteClicked()
{
    if (!m_db || !deleteBtn->isEnabled())
        return;
    const auto *cur = list->currentItem();
    const int executionId =
        cur ? cur->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0)
        return;

    // Same confirmation style as the skill/model panel delete.
    if (QMessageBox::question(
            this, tr("Delete Execution"),
            tr("Delete execution %1? It stays in the database and can "
               "be restored.")
                .arg(executionId))
        != QMessageBox::Yes)
        return;

    const int rc = acta_db_execution_delete(m_db, executionId);
    if (rc != ACTA_DB_OK) {
        QString msg;
        switch (rc) {
        case ACTA_DB_ERR_INVALID:
            // The row is running: the runner owns it, so it cannot be
            // deleted mid-run.
            msg = tr("Cannot delete a running execution.");
            break;
        case ACTA_DB_ERR_NOT_FOUND:
            msg = tr("Execution not found (already deleted?).");
            break;
        default:
            msg = tr("Could not delete the execution: %1")
                    .arg(QString::fromUtf8(acta_db_strerror(rc)));
        }
        QMessageBox::warning(this, tr("Delete Execution"), msg);
        return;
    }

    reload();
}

void ExecutionPanel::onExecuteRestoreClicked()
{
    if (!m_db || !restoreBtn->isEnabled())
        return;
    const auto *cur = list->currentItem();
    const int executionId =
        cur ? cur->data(0, RoleExecutionId).toInt() : 0;
    if (executionId == 0)
        return;

    const int rc = acta_db_execution_restore(m_db, executionId);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, tr("Restore Execution"),
            tr("Could not restore the execution: %1")
                .arg(QString::fromUtf8(acta_db_strerror(rc))));
        return;
    }
    // Status is untouched by the restore: a restored failed row is
    // still failed and can be retried with Run.
    QMessageBox::information(this, tr("Execution"),
                             tr("Execution %1 restored.").arg(executionId));
    reload();
}

void ExecutionPanel::onDeleteKeyPressed()
{
    // Same gating as updateActionBtnStates(): Delete soft-deletes a
    // live, non-running row; Restore undeletes a deleted row.
    if (deleteBtn->isEnabled())
        onExecuteDeleteClicked();
    else if (restoreBtn->isEnabled())
        onExecuteRestoreClicked();
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
        if (key->modifiers() == Qt::NoModifier) {
            if (key->key() == Qt::Key_Return
                    || key->key() == Qt::Key_Enter) {
                onReturnKeyPressed();
                return true;
            }
            // Delete soft-deletes the selection (or restores a deleted
            // row), mirroring the skill/model panel's Delete key.
            if (key->key() == Qt::Key_Delete) {
                onDeleteKeyPressed();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
