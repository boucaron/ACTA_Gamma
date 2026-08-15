#include "contextpanel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>

ContextPanel::ContextPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Context"));

    editor = new QTextEdit;
    editor->setPlaceholderText("Immutable input JSON...");
    lay->addWidget(editor);
}
