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

    // Persistent offline indicator + Retry button (shown only when offline).
    QWidget *m_offlineWidget = nullptr;
    QLabel *m_offlineBanner = nullptr;
    QPushButton *m_retryButton = nullptr;

    // Default DB location: writable AppData dir, created if needed (UR #9).
    static QString defaultDbPath();

    // Open an existing DB, or (first run, no file) create it and apply
    // the embedded schema.sql. Returns true on success; on failure sets
    // m_dbOk=false and fills m_lastError.
    bool openDatabaseOnce();

    // Create a fresh database at m_dbPath and apply the embedded schema.
    bool createDatabase();

    // Grey out / re-enable the four panels and show/hide the offline
    // banner according to dbAvailable() (UR #10).
    void syncPanels();

    // Bootstrap loop: open/create, and on failure offer the startup
    // modal (create / pick location / exit) (UR #32).
    void runDatabaseBootstrap();
};
