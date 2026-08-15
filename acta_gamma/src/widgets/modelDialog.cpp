#include "modelDialog.h"
#include "ui_modelDialog.h"

ModelDialog::ModelDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_modelDialog)
{
    ui->setupUi(this);
    setWindowTitle("Model");
    // connect your buttons, validators, etc. here
}

ModelDialog::~ModelDialog()
{
    delete ui;
}
