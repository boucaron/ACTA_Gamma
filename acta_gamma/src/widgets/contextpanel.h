#pragma once
#include <QWidget>
class QTreeWidget;
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

    // Open the (read-only) context dialog for `contextId`.
    void editContext(int contextId);

private slots:
    void onShowBtnClicked();
    void onListContextMenu(const QPoint &pos);
};
