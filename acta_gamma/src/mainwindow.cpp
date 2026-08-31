#include "mainwindow.h"
#include "widgets/skillpanel.h"
#include "widgets/modelpanel.h"
#include "widgets/contextpanel.h"
#include "widgets/executionpanel.h"

#include <QCoreApplication>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QStatusBar>

#include "widgets/skillpanel.h"
#include "widgets/modelpanel.h"
#include "widgets/contextpanel.h"
#include "widgets/executionpanel.h"


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // Database (RAII: m_db is closed at shutdown by its destructor,
    // acta_db_close with acta_db_force_close fallback).
    {
        const QString dbPath = QCoreApplication::applicationDirPath()
                               + QStringLiteral("/acta.db");
        int code = ACTA_DB_OK;
        QString msg;
        if (m_db.open(dbPath, ACTA_DB_OPEN_EXISTING, &msg, &code)) {
            statusBar()->showMessage(
                tr("Connected to %1").arg(dbPath), 3000);
        } else {
            // On open failure there is no live handle, so
            // acta_db_last_error() cannot be consulted; the surfaced
            // error is the acta_db_strerror of the error code.
            const QString last = m_db.valid() && m_db.lastError()
                                     ? QStringLiteral(": %1")
                                           .arg(m_db.lastError())
                                     : QString();
            statusBar()->showMessage(
                tr("Database error [%1]: %2%3 — %4")
                    .arg(code).arg(msg).arg(last).arg(dbPath));
            qWarning("acta_db_open(%s) failed: %s (code %d)",
                     qPrintable(dbPath), qPrintable(msg), code);
        }
    }

    auto central = new QWidget(this);
    auto root = new QVBoxLayout(central);

    // Logo header
    auto header = new QWidget;
    auto h = new QHBoxLayout(header);
    QLabel *logo = new QLabel;
    QPixmap pm(":/assets/logo.jpg");
    if(!pm.isNull()) logo->setPixmap(pm.scaledToHeight(32));
    QLabel *title = new QLabel("ACTA Gamma — LLMs as actions, not agents");
    h->addWidget(logo);
    h->addWidget(title);
    h->addStretch();
    root->addWidget(header);

    auto splitter = new QSplitter(Qt::Horizontal);
    auto left = new QWidget;
    auto leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new SkillPanel);
    leftLayout->addWidget(new ModelPanel);
    left->setMaximumWidth(320);

    auto right = new QWidget;
    auto rightLayout = new QVBoxLayout(right);
    rightLayout->addWidget(new ContextPanel);
    rightLayout->addWidget(new ExecutionPanel);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setSizes({320, 880});

    root->addWidget(splitter);
    setCentralWidget(central);
    resize(1200,700);
}
