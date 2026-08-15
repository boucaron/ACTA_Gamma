#include "skilldialog.h"
#include "ui_skillDialog.h"

SkillDialog::SkillDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_skillDialog)
{
    ui->setupUi(this);
    setWindowTitle("Skill");
    // connect your buttons, validators, etc. here
}

SkillDialog::~SkillDialog()
{
    delete ui;
}
