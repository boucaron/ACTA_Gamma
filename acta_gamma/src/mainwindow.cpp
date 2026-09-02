#include "mainwindow.h"
#include "widgets/skillpanel.h"
#include "widgets/modelPanel.h"
#include "widgets/contextpanel.h"
#include "widgets/executionpanel.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPixMap>
#include <QPushButton>
#include <QSplitter>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>

QString MainWindow::defaultDbPath()
{
    // Write the DB into a location we can actually write to (UR #9),
    // instead of next to the exe, which may be read-only.
    const QString baseDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(baseDir);
    return baseDir + QStringLiteral("/acta.db");
}

QString MainWindow::storedDbPath()
{
    QSettings s;
    return s.value(QStringLiteral("database/path")).toString();
}

void MainWindow::storeDbPath(const QString &path)
{
    QSettings s;
    s.setValue(QStringLiteral("database/path"), path);
    // Flush now: the value must survive to the next process, not just
    // to this one's exit-time sync.
    s.sync();
}

bool MainWindow::openDatabaseOnce()
{
    // Idempotent: release any stale handle before (re)opening.
    if (m_db.valid())
        m_db.close();
    m_dbOk = false;
    m_lastError.clear();

    int code = ACTA_DB_OK;
    QString msg;
    if (m_db.open(m_dbPath, ACTA_DB_OPEN_EXISTING, &msg, &code)) {
        m_dbOk = true;
        statusBar()->showMessage(tr("Connected to %1").arg(m_dbPath), 3000);
        return true;
    }

    // No usable DB here. The caller (runDatabaseBootstrap) offers the
    // startup modal: create a fresh DB, or open an existing valid file
    // (e.g. the user's acta.db with the schema). We deliberately do NOT
    // auto-create here, or we would silently shadow an existing DB.
    m_lastError =
        tr("Could not open the database: [%1] %2\n%3")
            .arg(code)
            .arg(msg)
            .arg(m_dbPath);
    qWarning("acta_db_open(%s) failed: %s (code %d)", qPrintable(m_dbPath),
             qPrintable(msg), code);
    return false;
}

bool MainWindow::createDatabase()
{
    int code = ACTA_DB_OK;
    QString msg;
    if (!m_db.open(m_dbPath, ACTA_DB_OPEN_CREATE, &msg, &code)) {
        qWarning("db create open failed: %s (code %d)", qPrintable(msg), code);
        return false;
    }

    // Apply the schema embedded as a Qt resource (UR #9).
    QFile schema(QStringLiteral(":/db/schema.sql"));
    if (!schema.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("cannot read embedded :/db/schema.sql");
        m_db.close();
        return false;
    }
    const QByteArray sql = schema.readAll();
    schema.close();
    const int rc = acta_db_exec(m_db.handle(), sql.constData());
    if (rc != ACTA_DB_OK) {
        qWarning("applying schema.sql failed: %s", acta_db_strerror(rc));
        m_db.close();
        return false;
    }
    return true;
}

void MainWindow::syncPanels()
{
    const bool ok = m_dbOk;
    if (m_skillPanel)
        m_skillPanel->setEnabled(ok);
    if (m_modelPanel)
        m_modelPanel->setEnabled(ok);
    if (m_contextPanel)
        m_contextPanel->setEnabled(ok);
    if (m_executionPanel)
        m_executionPanel->setEnabled(ok);
    if (m_offlineWidget)
        m_offlineWidget->setVisible(!ok);
}

void MainWindow::retryDatabase()
{
    runDatabaseBootstrap();
    syncPanels();
}

void MainWindow::runDatabaseBootstrap()
{
    // Loop so "Create database here" / "Choose another location" retries
    // are handled without unbounded recursion.
    for (;;) {
        if (openDatabaseOnce()) {
            // Remember whatever worked so the next launch opens it
            // directly (H6).
            storeDbPath(m_dbPath);
            return;
        }

        // Real failure: offer the startup modal (UR #32).
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("Database unavailable"));
        box.setText(m_lastError);
        QPushButton *createBtn =
            box.addButton(tr("Create database here"), QMessageBox::AcceptRole);
        QPushButton *pickBtn =
            box.addButton(tr("Choose database file"), QMessageBox::ActionRole);
        QPushButton *quitBtn =
            box.addButton(tr("Exit"), QMessageBox::RejectRole);
        box.exec();

        const auto *clicked = box.clickedButton();
        if (clicked == quitBtn) {
            QCoreApplication::exit(0);
            return;
        }
        if (clicked == createBtn) {
            // Fresh database: drop any stray file, create + apply schema.
            QFile::remove(m_dbPath);
            if (createDatabase()) {
                m_dbOk = true;
                storeDbPath(m_dbPath);
                statusBar()->showMessage(
                    tr("Created database %1").arg(m_dbPath), 3000);
                return;
            }
            continue; // still failed -> show the modal again
        }
        if (clicked == pickBtn) {
            // Pick the actual database file, not just a folder.
            const QString file = QFileDialog::getOpenFileName(
                this, tr("Choose database file"),
                QFileInfo(m_dbPath).absolutePath(),
                tr("SQLite databases (*.db);;All files (*)"));
            if (file.isEmpty())
                return; // stay offline; the banner offers Retry
            m_dbPath = file;
            continue;
        }
        return; // unrecognized -> stay offline
    }
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // Database bootstrap (UR #9, #32): open an existing DB, or create it
    // and apply the schema on first run. On a real failure this shows the
    // startup modal (create / pick location / exit) and leaves m_dbOk
    // false; the offline banner + Retry then keep the state visible.
    m_dbPath = defaultDbPath();
    // Prefer the database path remembered from a previous session (H6);
    // fall back to the default location when it is stale, in which case
    // the bootstrap modal above handles it as before.
    const QString stored = storedDbPath();
    if (!stored.isEmpty() && QFile::exists(stored))
        m_dbPath = stored;
    runDatabaseBootstrap();

    auto central = new QWidget(this);
    auto root = new QVBoxLayout(central);

    // Logo header
    auto header = new QWidget;
    auto h = new QHBoxLayout(header);
    QLabel *logo = new QLabel;
    QPixmap pm(":/assets/logo.jpg");
    if (!pm.isNull())
        logo->setPixmap(pm.scaledToHeight(32));
    QLabel *title = new QLabel("ACTA Gamma — LLMs as actions, not agents");
    h->addWidget(logo);
    h->addWidget(title);
    h->addStretch();
    root->addWidget(header);

    // Persistent offline indicator (UR #32, #10): shown only while the DB
    // is unavailable, with a Retry action.
    m_offlineWidget = new QWidget;
    auto *offlineRow = new QHBoxLayout(m_offlineWidget);
    m_offlineBanner =
        new QLabel(tr("Database unavailable — the panels are offline."));
    m_retryButton = new QPushButton(tr("Retry"));
    offlineRow->addWidget(m_offlineBanner);
    offlineRow->addWidget(m_retryButton);
    offlineRow->addStretch();
    root->addWidget(m_offlineWidget);

    // UR #29: the left pane used to have setMaximumWidth(320) *and*
    // splitter sizes {320, 880}, so the user could never widen it.
    // Drop the hard max and keep only a sane minimum on both sides; the
    // splitter (with the initial sizes below) does the rest.
    auto splitter = new QSplitter(Qt::Horizontal);
    m_splitter = splitter;
    auto left = new QWidget;
    auto leftLayout = new QVBoxLayout(left);
    m_skillPanel = new SkillPanel(m_db.handle());
    m_modelPanel = new ModelPanel(m_db.handle());
    leftLayout->addWidget(m_skillPanel);
    leftLayout->addWidget(m_modelPanel);
    left->setMinimumWidth(160);

    auto right = new QWidget;
    auto rightLayout = new QVBoxLayout(right);
    m_contextPanel = new ContextPanel(m_db.handle());
    m_executionPanel = new ExecutionPanel(m_db.handle());
    rightLayout->addWidget(m_contextPanel);
    rightLayout->addWidget(m_executionPanel);
    right->setMinimumWidth(160);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setSizes({320, 880});
    // When the window is resized, give the extra space to the right
    // (context/execution) side.
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter);
    setCentralWidget(central);

    connect(m_retryButton, &QPushButton::clicked, this,
            &MainWindow::retryDatabase);
    syncPanels();

    // Window-state persistence (UR #40): restore the window geometry,
    // the splitter sizes, and the "Show deleted items" flags from a
    // previous session; fall back to the default geometry on first run.
    {
        QSettings s;
        const QByteArray geom =
            s.value(QStringLiteral("window/geometry")).toByteArray();
        if (geom.isEmpty())
            resize(1200, 700);
        else
            restoreGeometry(geom);

        const QByteArray splitterState =
            s.value(QStringLiteral("window/splitter")).toByteArray();
        if (!splitterState.isEmpty())
            splitter->restoreState(splitterState);

        // The panel constructors already reloaded with the default
        // (unchecked) state; setChecked() re-runs reload() only when it
        // actually toggles, so this is cheap on the common no-op path.
        if (m_skillPanel->showDeletedCheck
                && s.value(QStringLiteral("window/showDeletedSkill"), false)
                       .toBool() != m_skillPanel->showDeletedCheck->isChecked())
            m_skillPanel->showDeletedCheck->setChecked(true);
        if (m_modelPanel->showDeletedCheck
                && s.value(QStringLiteral("window/showDeletedModel"), false)
                       .toBool() != m_modelPanel->showDeletedCheck->isChecked())
            m_modelPanel->showDeletedCheck->setChecked(true);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Window-state persistence (UR #40): remember the window geometry,
    // the splitter sizes, and the "Show deleted items" flags so the next
    // launch starts exactly where this one ended.
    if (m_splitter && m_skillPanel && m_modelPanel) {
        QSettings s;
        s.setValue(QStringLiteral("window/geometry"), saveGeometry());
        s.setValue(QStringLiteral("window/splitter"),
                    m_splitter->saveState());
        s.setValue(QStringLiteral("window/showDeletedSkill"),
                    m_skillPanel->showDeletedCheck->isChecked());
        s.setValue(QStringLiteral("window/showDeletedModel"),
                    m_modelPanel->showDeletedCheck->isChecked());
        s.sync();
    }
    QMainWindow::closeEvent(event);
}
