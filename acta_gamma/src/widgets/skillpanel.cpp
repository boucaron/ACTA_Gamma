#include "skillpanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

SkillPanel::SkillPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Skill"));

    combo = new QComboBox;
    combo->addItems({"review_pr/v1","review_pr/v2","extract_entities/v1"});
    lay->addWidget(combo);

    loadBtn = new QPushButton("Load Skill");
    lay->addWidget(loadBtn);
}
