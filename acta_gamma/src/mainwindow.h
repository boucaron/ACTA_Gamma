#pragma once
#include <QMainWindow>
class SkillPanel;
class ModelPanel;
class ContextPanel;
class ExecutionPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
};
