#pragma once
#include <QWidget>
class QTextEdit;
class QPushButton;

class ContextPanel : public QWidget {
public:
    explicit ContextPanel(QWidget *parent = nullptr);
    QTextEdit *editor;

    QPushButton *showBtn;

private slots:
    void onShowBtnClicked();
};
