#pragma once
#include <QWidget>

class QComboBox;
class QPushButton;

class ModelPanel : public QWidget {
public:
    explicit ModelPanel(QWidget *parent = nullptr);
    QComboBox *combo;
    QPushButton *loadBtn;

private slots:
    void onLoadBtnClicked();
};
