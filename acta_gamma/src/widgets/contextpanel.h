#pragma once
#include <QWidget>
class QTextEdit;
class ContextPanel : public QWidget {
public:
    explicit ContextPanel(QWidget *parent = nullptr);
    QTextEdit *editor;
};
