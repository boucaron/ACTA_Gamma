#pragma once
#include <QWidget>
class QComboBox;
class ModelPanel : public QWidget {
public:
    explicit ModelPanel(QWidget *parent = nullptr);
    QComboBox *combo;
};
