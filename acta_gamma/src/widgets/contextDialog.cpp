#include "contextDialog.h"
#include "ui_contextDialog.h"

#include <QDateTime>
#include <QLocale>

#include <QtGlobal>

namespace {

QString utf8(const char *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// created_at comes back from SQLite's now() as "yyyy-MM-dd HH:mm:ss".
QDateTime toDateTime(const char *iso)
{
    if (!iso || !*iso)
        return {};
    const QString s = QString::fromUtf8(iso);
    static const QList<Qt::DateFormat> formats = {
        Qt::ISODateWithMs, Qt::ISODate,
    };
    for (auto f : formats) {
        const QDateTime dt = QDateTime::fromString(s, f);
        if (dt.isValid())
            return dt;
    }
    return QLocale::c().toDateTime(s, "yyyy-MM-dd HH:mm:ss");
}

} // namespace

ContextDialog::ContextDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_contextDialog)
{
    ui->setupUi(this);
    setWindowTitle("Context");
    // connect your buttons, validators, etc. here
}

ContextDialog::~ContextDialog()
{
    delete ui;
}

void ContextDialog::editContext(db_t *db, int contextId)
{
    int err = ACTA_DB_OK;
    context_t *c = acta_db_context_get(db, contextId, &err);
    if (!c) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_get(%d) failed: %s", contextId,
                     acta_db_strerror(err));
        return;
    }

    // Data
    ui->typeLineEdit->setText(utf8(c->type));
    ui->contentTextEdit->setPlainText(utf8(c->content));

    // Metadata
    ui->contextHashLineEdit->setText(utf8(c->content_hash));
    ui->dateTimeEdit->setDateTime(toDateTime(c->created_at));
    ui->metaDataTextEdit->setPlainText(utf8(c->metadata));

    acta_db_context_free(c);
}
