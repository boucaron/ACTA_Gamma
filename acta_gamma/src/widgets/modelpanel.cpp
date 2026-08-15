#include "modelPanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "modelDialog.h"

ModelPanel::ModelPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Model"));

    combo = new QComboBox;
    combo->addItems({"llamacpp/local-7b","openai/gpt-4o-mini"});
    lay->addWidget(combo);

    loadBtn = new QPushButton("Load Model");
    lay->addWidget(loadBtn);


    // callback
    connect(loadBtn, &QPushButton::clicked, this, &ModelPanel::onLoadBtnClicked);
}



void ModelPanel::onLoadBtnClicked()
{
    ModelDialog dlg(this);
    if(dlg.exec() == QDialog::Accepted){
      // TODO
    }
}
