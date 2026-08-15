#include "contextPanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

#include "contextDialog.h"

ContextPanel::ContextPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Context"));

    editor = new QTextEdit;
    editor->setPlaceholderText("Immutable input JSON...");
    lay->addWidget(editor);

    showBtn = new QPushButton("Show");
    lay->addWidget(showBtn);


    // callback
    connect(showBtn, &QPushButton::clicked, this, &ContextPanel::onShowBtnClicked);
}


void ContextPanel::onShowBtnClicked()
{
    ContextDialog dlg(this);
    if(dlg.exec() == QDialog::Accepted){
      // TODO
    }
}
