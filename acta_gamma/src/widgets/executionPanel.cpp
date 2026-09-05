#include "executionPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QKeySequence>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>

#include "executionDialog.h"
#include "executionCreateDialog.h"
#include "util.h"

namespace {
const int RoleExecutionId = Qt::UserRole;

// Last stderr line that parses as a JSON object with a non-empty
// "message" field — the runner's single-line error contract
// (error / code / message, per runner_util.h). Empty when no line
// parses; the caller falls back to raw stderr.
QString parseRunnerError(const QByteArray &stderrAll)
{
    for (const QString &line :
             QString::fromUtf8(stderrAll).split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        QJsonParseError parseErr{};
        const QJsonDocument doc =
            QJsonDocument::fromJson(line.toUtf8(), &parseErr);
        if (doc.isNull() || !doc.isObject())
            continue;
        const QString message = doc.object().value("message").toString();
        if (!message.isEmpty())
            return message;
    }
    return QString();
}
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

    // Centered placeholder over the blank log list (P5 / UR #31);
    // shown/hidden in showExecutionLogs().
    emptyLogLabel = makeEmptyStateLabel(
        logList, tr("No log lines for this execution"));

    // The log list already shows all four log columns inline, so the
    // redundant "Show Log Details" button and log context menu were
    // dropped (P5 / UR #41).

    // Icon-only toolbar row (P2 / UR #22), matching the other panels:
    // tooltips carry the meaning, accelerators are Alt+letter (UR #39).
    // "New" opens the create dialog (a new row lands in "pending"; the
    // runner that moves it through start/complete/fail lives in
    // acta_runner — the in-app "Run" button is Plan D, not yet shipped).
    // "Show" opens the execution dialog for the selected execution row;
    // the context menu and double-click / Enter do the same (UR #33).
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
    // "Run" (R1 / Plan D): spawns acta_runner run <id> for the selected
    // row and polls the DB for live status + phase log rows (UR #18
    // remainder, UR #44).
    runBtn = makeActionButton(
        style()->standardIcon(QStyle::SP_MediaPlay),
        tr("Run the selected execution"),
        QKeySequence(Qt::ALT | Qt::Key_R));
    btnRow->addWidget(runBtn);
    lay->addLayout(btnRow);

    // callbacks
    connect(newExecutionBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onNewBtnClicked);
    connect(showDetailsBtn, &QPushButton::clicked, this, [this] {
        onExecutionDoubleClicked(list->currentItem(), 0);
    });
    connect(runBtn, &QPushButton::clicked, this,
            &ExecutionPanel::onRunBtnClicked);

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
    if (m_runner) {
        // The database switched underneath the runner: kill it rather
        // than let it keep writing to the stale database.
        if (m_runner->state() != QProcess::NotRunning) {
            m_runner->kill();
            m_runner->waitForFinished(2000);
        }
        m_runner->deleteLater();
        m_runner = nullptr;
    }
    m_runningExecutionId = 0;
}

void ExecutionPanel::updateRunBtnState()
{
    bool canRun = m_db != nullptr && m_runner == nullptr;
    if (canRun) {
        const auto *cur = list->currentItem();
        canRun = cur != nullptr
            && cur->text(1) == QLatin1String(ACTA_EXEC_STATUS_PENDING);
    }
    runBtn->setEnabled(canRun);
}

QString ExecutionPanel::findRunnerExe() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &cand :
             {appDir + QStringLiteral("/acta_runner"),
              appDir + QStringLiteral("/acta_runner.exe")}) {
        if (QFile::exists(cand))
            return cand;
    }
    const QString found =
        QStandardPaths::findExecutable(QStringLiteral("acta_runner"));
    return found; // empty when not found
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
    if (!m_db || m_runner)
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
    // The runner atomically refuses non-pending rows via start() anyway;
    // this is a UI convenience (the button is only enabled for pending
    // rows, but the context menu / shortcut can fire on a stale
    // selection).
    const QString status =
        exec->status ? QString::fromUtf8(exec->status) : QString();
    acta_db_execution_free(exec);
    if (status != QLatin1String(ACTA_EXEC_STATUS_PENDING))
        return;

    const QString exe = findRunnerExe();
    if (exe.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Run"),
            tr("acta_runner was not found. Build it in acta_runner/ and "
               "place the executable next to the app, or add it to PATH."));
        return;
    }

    m_runningExecutionId = executionId;
    m_runner = new QProcess(this);
    connect(m_runner, &QProcess::finished, this,
            &ExecutionPanel::onRunnerFinished);
    connect(m_runner, &QProcess::errorOccurred, this,
            &ExecutionPanel::onRunnerError);
    // Poll the DB while the runner is active (Plan D: the database is
    // the message bus; WAL supports the concurrent reader here).
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1500);
    connect(m_pollTimer, &QTimer::timeout, this, &ExecutionPanel::onPollTick);
    m_pollTimer->start();
    // No --api_key (the runner resolves --api_key -> $OPENAI_API_KEY ->
    // configuration.api_key), no -v, no --timeout (default 300 s).
    m_runner->start(
        exe,
        {QStringLiteral("run"),
         QString::number(executionId),
         QStringLiteral("--db"),
         m_dbPath});
    refreshRunningRow();
    updateRunBtnState();
}

void ExecutionPanel::onRunnerFinished(int exitCode, QProcess::ExitStatus)
{
    if (!m_runner)
        return;
    m_pollTimer->stop();
    m_pollTimer->deleteLater();
    m_pollTimer = nullptr;

    // Final targeted refresh; the row is already completed/failed in the
    // DB, the refresh only syncs the display.
    refreshRunningRow();
    refreshRunningLogs();

    if (exitCode != 0) {
        // The execution row already carries the error; this is purely
        // informational (P4 error-surfacing style).
        const QString detail =
            parseRunnerError(m_runner->readAllStandardError());
        QMessageBox::warning(
            this,
            tr("Execution failed"),
            detail.isEmpty()
                ? tr("The runner exited with code %1").arg(exitCode)
                : detail);
    }

    m_runner->deleteLater();
    m_runner = nullptr;
    m_runningExecutionId = 0;
    updateRunBtnState();
}

void ExecutionPanel::onRunnerError(QProcess::ProcessError)
{
    if (!m_runner)
        return;
    m_pollTimer->stop();
    m_pollTimer->deleteLater();
    m_pollTimer = nullptr;
    QMessageBox::warning(
        this,
        tr("Run"),
        tr("Could not start acta_runner: %1").arg(m_runner->errorString()));
    m_runner->deleteLater();
    m_runner = nullptr;
    m_runningExecutionId = 0;
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
        logList->setModel(emptyModel);
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
    logList->setModel(model);

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
    auto *aRun = menu.addAction(tr("Run"));
    aRun->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    aRun->setToolTip(tr("Run the selected execution"));
    aRun->setEnabled(runBtn->isEnabled());
    connect(aRun, &QAction::triggered, this, [this] { onRunBtnClicked(); });
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
