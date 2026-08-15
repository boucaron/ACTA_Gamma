#include "modelPanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>

ModelPanel::ModelPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Model"));

    combo = new QComboBox;
    combo->addItems({"llamacpp/local-7b","openai/gpt-4o-mini"});
    lay->addWidget(combo);
}
