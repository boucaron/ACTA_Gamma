#pragma once
#include <QWidget>
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QPushButton;

#include "acta_db.h"

class ContextPanel : public QWidget {
public:
    explicit ContextPanel(db_t *db = nullptr, QWidget *parent = nullptr);
    QTreeWidget *list;
    QTextEdit *editor;

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
    void onShowBtnClicked();
    void onListContextMenu(const QPoint &pos);
};
