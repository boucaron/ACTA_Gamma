#pragma once
#include <QMainWindow>
#include "dbhandle.h"
class SkillPanel;
class ModelPanel;
class ContextPanel;
class ExecutionPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    DbHandle m_db; // RAII: closed at shutdown (destructor)
};
