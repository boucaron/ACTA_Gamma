#ifndef CONTEXTDIALOG_H
#define CONTEXTDIALOG_H

#include <QDialog>
#include "ui_contextDialog.h"

#include "acta_db.h"

class ContextDialog : public QDialog
{
    Q_OBJECT
public:
    // What the dialog is showing / doing. Contexts are immutable:
    // there is no edit mode, only create and view.
    enum class Mode {
        New,      // empty form, Save creates the context
        ReadOnly, // viewer (Show), Close only
    };

    explicit ContextDialog(QWidget *parent = nullptr);
    ~ContextDialog();

    // Read-only view: fetch context `contextId` and fill every field.
    void editContext(db_t *db, int contextId);

    // New context: clear every field, Save button creates the context
    // (content_hash is computed from the content, SHA-256 hex).
    void newContext(db_t *db);

private:
    Ui_contextDialog *ui;
    db_t *m_db = nullptr;
    Mode m_mode = Mode::ReadOnly;

    // Apply mode: button bar (Close / Save|Close) and field
    // read-only flags.
    void setMode(Mode mode);

    // Save button slot: create the context from the dialog fields.
    void onSaveClicked();
};

#endif
