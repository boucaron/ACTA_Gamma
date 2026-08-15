#pragma once
#include <QWidget>

class QComboBox;
class QPushButton;

class SkillPanel : public QWidget {
    Q_OBJECT
public:
    explicit SkillPanel(QWidget *parent = nullptr);

    QComboBox *combo;
    QPushButton *loadBtn;

private slots:
    void onLoadBtnClicked();

};
