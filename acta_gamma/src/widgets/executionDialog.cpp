#include "executionDialog.h"
#include "ui_executionDialog.h"

ExecutionDialog::ExecutionDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_executionDialog)
{
    ui->setupUi(this);
    setWindowTitle("Execution");
    // connect your buttons, validators, etc. here
}

ExecutionDialog::~ExecutionDialog()
{
    delete ui;
}
