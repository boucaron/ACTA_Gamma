#include "executionpanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

ExecutionPanel::ExecutionPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Execution"));

    runBtn = new QPushButton("Run One-Shot");
    lay->addWidget(runBtn);

    log = new QTextEdit;
    log->setReadOnly(true);
    lay->addWidget(log);
}
