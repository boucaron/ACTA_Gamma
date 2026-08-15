#include "contextDialog.h"
#include "ui_contextDialog.h"

ContextDialog::ContextDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_contextDialog)
{
    ui->setupUi(this);
    setWindowTitle("Context");
    // connect your buttons, validators, etc. here
}

ContextDialog::~ContextDialog()
{
    delete ui;
}
