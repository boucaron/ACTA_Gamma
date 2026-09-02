#pragma once
#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QPushButton;
class QLineEdit;

#include "acta_db.h"

class ContextPanel : public QWidget {
public:
    explicit ContextPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QLineEdit *filterEdit; // case-insensitive substring filter (H4 / UR #38)
    QTextEdit *editor;

    QPushButton *newBtn;
    QPushButton *showBtn;

    // Rebuild the list from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

private:
    db_t *m_db;

    // Fill the textarea below the list with the content of the selected
    // context (or clear it when the selection leaves a context row).
    void showContext(QTreeWidgetItem *item);

    // Open the (read-only) context dialog for `contextId`.
    void editContext(int contextId);

private slots:
    void onNewBtnClicked();
    void onShowBtnClicked();
    void onListContextMenu(const QPoint &pos);
    // Keyboard accelerator (UR #39), active while the list has focus:
    // Enter opens the read-only dialog, the same as the Show button.
    // Intercepted in eventFilter() so it fires before the list's own key
    // handling (which would start inline editing on Return).
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onReturnKeyPressed();
};
