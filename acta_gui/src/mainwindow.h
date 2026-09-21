#pragma once
#include <QMainWindow>
#include "dbhandle.h"

class SkillPanel;
class ModelPanel;
class ContextPanel;
class ExecutionPanel;
class QWidget;
class QLabel;
class QPushButton;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // True when a live db handle is open; false while offline (UR #10).
    // Lets panels / other widgets show an explicit disabled state instead
    // of a silent empty tree.
    bool dbAvailable() const { return m_dbOk; }

    // Re-attempt to open/create the database (the offline "Retry" action).
    void retryDatabase();

    // Load a different database file (Database menu "Load Database…"):
    // file dialog, open it, and re-point the panels at the new handle.
    void loadDatabase();

private:
    DbHandle m_db; // RAII: closed at shutdown (destructor)
    bool m_dbOk = false;
    QString m_dbPath;
    QString m_lastError;

    // Panels, held so we can grey them out while the DB is offline.
    SkillPanel *m_skillPanel = nullptr;
    ModelPanel *m_modelPanel = nullptr;
    ContextPanel *m_contextPanel = nullptr;
    ExecutionPanel *m_executionPanel = nullptr;

    // Held for window-state persistence (UR #40): the splitter state is
    // saved in closeEvent().
    QSplitter *m_splitter = nullptr;

    // Persistent offline indicator + Retry button (shown only when offline).
    QWidget *m_offlineWidget = nullptr;
    QLabel *m_offlineBanner = nullptr;
    QPushButton *m_retryButton = nullptr;

    // Default DB location: writable AppData dir, created if needed (UR #9).
    // Consults the config file's "db" first (docs/plans/
    // acta-config-file.md, work item 3); a readable-but-malformed
    // config file is a hard error: returns an empty string and sets
    // *error (may be NULL).
    static QString defaultDbPath(QString *error = nullptr);

    // Preferences (H6): the database path remembered from a previous
    // session; empty when nothing is stored yet.
    static QString storedDbPath();

    // Preferences (H6): persist the database path so the next launch
    // opens it directly instead of asking the user again.
    static void storeDbPath(const QString &path);

    // Open an existing DB, or (first run, no file) create it and apply
    // the embedded schema.sql. Returns true on success; on failure sets
    // m_dbOk=false and fills m_lastError.
    bool openDatabaseOnce();

    // Create a fresh database at m_dbPath and apply the embedded schema.
    bool createDatabase();

    // Grey out / re-enable the four panels and show/hide the offline
    // banner according to dbAvailable() (UR #10).
    void syncPanels();

    // Re-point all four panels at the currently open db handle. Needed
    // after any successful (re)open: open() hands out a fresh db_t*,
    // so the panels' captured handles would otherwise go stale.
    void applyDbToPanels();

    // Bootstrap loop: open/create, and on failure offer the startup
    // modal (create / pick location / exit) (UR #32).
    void runDatabaseBootstrap();

protected:
    // Window-state persistence (UR #40): save the window geometry, the
    // splitter sizes, and the "Show trash" flags to QSettings on
    // close; the constructor restores them on the next launch.
    void closeEvent(QCloseEvent *event) override;
};
