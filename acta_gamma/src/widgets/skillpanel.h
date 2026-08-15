#pragma once
#include <QWidget>

class QComboBox;
class QPushButton;

class SkillPanel : public QWidget {
public:
    explicit SkillPanel(QWidget *parent = nullptr);
    QComboBox *combo;
    QPushButton *loadBtn;
};
