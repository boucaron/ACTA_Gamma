#pragma once
#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QPushButton;
class QLineEdit;
class QLabel;

#include "acta_db.h"

class ContextPanel : public QWidget {
public:
    explicit ContextPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QLabel *emptyLabel; // centered placeholder when the list is empty (P5 / UR #31)
    QLineEdit *filterEdit; // case-insensitive substring filter (H4 / UR #38)
    QTextEdit *editor;

    QPushButton *newBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

private:
    db_t *m_db;

    // Fill the textarea below the list with the content of the selected
    // context (or clear it when the selection leaves a context row).
    // The inline editor is the only detail view: contexts are immutable,
    // so the read-only dialog was dropped (P5 / UR #41).
    void showContext(QTreeWidgetItem *item);

private:
    // Keep emptyLabel centered when the viewport resizes (P5 / UR #31);
    // the event passes through to the viewport.
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onNewBtnClicked();
};
