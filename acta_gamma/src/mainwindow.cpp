#include "mainwindow.h"
#include "widgets/skillpanel.h"
#include "widgets/modelpanel.h"
#include "widgets/contextpanel.h"
#include "widgets/executionpanel.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>

#include "widgets/skillpanel.h"
#include "widgets/modelpanel.h"
#include "widgets/contextpanel.h"
#include "widgets/executionpanel.h"


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    auto central = new QWidget(this);
    auto root = new QVBoxLayout(central);

    // Logo header
    auto header = new QWidget;
    auto h = new QHBoxLayout(header);
    QLabel *logo = new QLabel;
    QPixmap pm(":/assets/logo.jpg");
    if(!pm.isNull()) logo->setPixmap(pm.scaledToHeight(32));
    QLabel *title = new QLabel("ACTA Gamma — LLMs as actions, not agents");
    h->addWidget(logo);
    h->addWidget(title);
    h->addStretch();
    root->addWidget(header);

    auto splitter = new QSplitter(Qt::Horizontal);
    auto left = new QWidget;
    auto leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new SkillPanel);
    leftLayout->addWidget(new ModelPanel);
    left->setMaximumWidth(320);

    auto right = new QWidget;
    auto rightLayout = new QVBoxLayout(right);
    rightLayout->addWidget(new ContextPanel);
    rightLayout->addWidget(new ExecutionPanel);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setSizes({320, 880});

    root->addWidget(splitter);
    setCentralWidget(central);
    resize(1200,700);
}
