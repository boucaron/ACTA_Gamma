#include "executionPanel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

#include "executionDialog.h"

ExecutionPanel::ExecutionPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Execution"));

    runBtn = new QPushButton("Run One-Shot");
    lay->addWidget(runBtn);

    log = new QTextEdit;
    log->setReadOnly(true);
    lay->addWidget(log);


    
    showBtn = new QPushButton("Show");
    lay->addWidget(showBtn);


    // callback
    connect(showBtn, &QPushButton::clicked, this, &ExecutionPanel::onShowBtnClicked);
}


void ExecutionPanel::onShowBtnClicked()
{
    ExecutionDialog dlg(this);
    if(dlg.exec() == QDialog::Accepted){
      // TODO
    }
}

