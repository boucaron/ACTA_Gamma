#include "skillpanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "skilldialog.h"

SkillPanel::SkillPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Skill"));

    combo = new QComboBox;
    combo->addItems({"review_pr/v1","review_pr/v2","extract_entities/v1"});
    lay->addWidget(combo);

    loadBtn = new QPushButton("Load Skill");
    lay->addWidget(loadBtn);


    // callback
    connect(loadBtn, &QPushButton::clicked, this, &SkillPanel::onLoadBtnClicked);
}


void SkillPanel::onLoadBtnClicked()
{
    SkillDialog dlg(this);
    if(dlg.exec() == QDialog::Accepted){
      // TODO
    }
}