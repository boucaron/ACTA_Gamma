#pragma once

#include <QWidget>
class QPushButton;
class QTextEdit;


class ExecutionPanel : public QWidget {
public:
    explicit ExecutionPanel(QWidget *parent = nullptr);
    QPushButton *runBtn;
    QTextEdit *log;

     QPushButton *showBtn;

private slots:
    void onShowBtnClicked();
};
